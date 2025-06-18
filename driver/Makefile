KERNEL_VERSION=$(shell uname -r | cut -d '-' -f1)
KERNEL_SRC ?= ../kernel/linux-$(KERNEL_VERSION)


obj-m += edu_drv.o

all: build

build:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
