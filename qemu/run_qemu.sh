#!/bin/bash
qemu-system-x86_64 \
  -m 512M \
  -kernel ../bzImage \
  -initrd ../initramfs.cpio.gz \
  -append "console=ttyS0 root=/dev/ram rw" \
  -nographic \
  -device edu \
  -virtfs local,path=../driver,mount_tag=host0,security_model=none,readonly \
  -fsdev local,id=myid,path=../driver,security_model=none \
  -device virtio-9p-pci,fsdev=myid,mount_tag=host0