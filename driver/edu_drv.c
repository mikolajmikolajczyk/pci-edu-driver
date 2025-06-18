#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#define EDU_IOCTL_CARD_IDENTIFICATION _IOR('e', 1, u32)
#define EDU_IOCTL_CARD_LIVENESS _IOWR('e', 2, u32)
#define EDU_IOCTL_CARD_FACTORIAL _IOWR('e', 3, u32)
#define EDU_IOCTL_CARD_FACTORIAL_IRQ _IOWR('e', 4, u32)


#define DRIVER_NAME "edu"
#define EDU_VENDOR_ID 0x1234  // vendor from QEMU's edu.c
#define EDU_DEVICE_ID 0x11e8  // device from QEMU's edu.c


static struct pci_device_id edu_pci_ids[] = {
    { PCI_DEVICE(EDU_VENDOR_ID, EDU_DEVICE_ID), },
    { 0, }
};
MODULE_DEVICE_TABLE(pci, edu_pci_ids);

struct edu_dev {
    void __iomem *mmio_base;
    struct miscdevice miscdev;
    wait_queue_head_t wq;
    bool factorial_ready;
};


static irqreturn_t edu_interrupt(int irq, void *dev_id)
{
    struct edu_dev *edu = dev_id;
    u32 status = ioread32(edu->mmio_base + 0x24); // interrupt status register

    if (status & 0x01) {
        iowrite32(0x01, edu->mmio_base + 0x64); // acknowledge interrupt (clear)
        edu->factorial_ready = true;
        wake_up_interruptible(&edu->wq);
        return IRQ_HANDLED;
    }

    return IRQ_NONE;
}

static ssize_t edu_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    struct edu_dev *edu = container_of(file->private_data, struct edu_dev, miscdev);
    u32 val = ioread32(edu->mmio_base + 0x00);
    return simple_read_from_buffer(buf, count, ppos, &val, sizeof(val));
}

static ssize_t edu_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    pr_warn("edu: write attempt denied\n");
    return -EPERM;
}

static long edu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct edu_dev *edu = container_of(file->private_data, struct edu_dev, miscdev);
    u32 val;

    switch (cmd) {
    case EDU_IOCTL_CARD_IDENTIFICATION:
        val = ioread32(edu->mmio_base + 0x00);
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            return -EFAULT;
        return 0;
    case EDU_IOCTL_CARD_LIVENESS:
        if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
            return -EFAULT;
        iowrite32(val, edu->mmio_base + 0x04);

        val = ioread32(edu->mmio_base + 0x04);
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            return -EFAULT;

        return 0;
    case EDU_IOCTL_CARD_FACTORIAL:
        if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
            return -EFAULT;
        // Here we set factorial number
        iowrite32(val, edu->mmio_base + 0x08);

        // Here we set status & 0x01 to start computation
        u32 status = ioread32(edu->mmio_base + 0x20);
        iowrite32(status & 0x01, edu->mmio_base + 0x20);

        // Now all we have to do is wait for computation to finish
        while (ioread32(edu->mmio_base + 0x20) & 0x01)
            cpu_relax();

        val = ioread32(edu->mmio_base + 0x08);
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            return -EFAULT;
        return 0;
    case EDU_IOCTL_CARD_FACTORIAL_IRQ:
        if (copy_from_user(&val, (void __user *)arg, sizeof(val)))
            return -EFAULT;
        // Here we write factorial number to register
        iowrite32(val, edu->mmio_base + 0x08);

        // Here we set status and irq bit to start computation
        iowrite32(0x81, edu->mmio_base + 0x20);

        wait_event_interruptible(edu->wq, edu->factorial_ready);

        val = ioread32(edu->mmio_base + 0x08);
        edu->factorial_ready = false;
        if (copy_to_user((void __user *)arg, &val, sizeof(val)))
            return -EFAULT;

        return 0;
    default:
        return -EINVAL;
    }
}

static const struct file_operations edu_fops = {
    .owner          = THIS_MODULE,
    .read           = edu_read,
    .write          = edu_write,
    .unlocked_ioctl = edu_ioctl,
};

static int edu_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    int err;
    struct edu_dev *edu;
    resource_size_t mmio_start, mmio_len;

    pr_info("edu: probing device\n");

    err = pci_enable_device(pdev);
    if (err)
        return err;

    pci_set_master(pdev);

    err = pci_request_regions(pdev, DRIVER_NAME);
    if (err)
        goto disable_dev;

    edu = devm_kzalloc(&pdev->dev, sizeof(*edu), GFP_KERNEL);
    if (!edu) {
        err = -ENOMEM;
        goto release_regions;
    }

    mmio_start = pci_resource_start(pdev, 0);
    mmio_len = pci_resource_len(pdev, 0);
    edu->mmio_base = devm_ioremap(&pdev->dev, mmio_start, mmio_len);
    if (!edu->mmio_base) {
        err = -EIO;
        goto release_regions;
    }

    pci_set_drvdata(pdev, edu);

    pr_info("edu: mapped MMIO at %p\n", edu->mmio_base);
    pr_info("edu: read back = 0x%X\n", ioread32(edu->mmio_base));

    init_waitqueue_head(&edu->wq);
    edu->factorial_ready = false;

    err = request_irq(pdev->irq, edu_interrupt, 0, DRIVER_NAME, edu);
    if (err) {
        dev_err(&pdev->dev, "failed to request IRQ\n");
        goto release_regions;
    }


    edu->miscdev.minor = MISC_DYNAMIC_MINOR;
    edu->miscdev.name = DRIVER_NAME;
    edu->miscdev.fops = &edu_fops;
    err = misc_register(&edu->miscdev);
    if (err) {
        dev_err(&pdev->dev, "failed to register misc device\n");
        goto release_regions;
    }

    return 0;

release_regions:
    pci_release_regions(pdev);
disable_dev:
    pci_disable_device(pdev);
    return err;
}

static void edu_remove(struct pci_dev *pdev)
{
    pr_info("edu: removing device\n");
    free_irq(pdev->irq, pci_get_drvdata(pdev));
    pci_release_regions(pdev);
    pci_disable_device(pdev);
}

static struct pci_driver edu_driver = {
    .name = DRIVER_NAME,
    .id_table = edu_pci_ids,
    .probe = edu_probe,
    .remove = edu_remove,
};

module_pci_driver(edu_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mikołaj Mikołajczyk");
MODULE_DESCRIPTION("Minimal driver for qemu edu device");
