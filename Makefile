.PHONY: kernel driver initramfs all qemu user
all: kernel driver user initramfs

kernel:
	cd kernel && ./build_kernel.sh

user:
	cd user && make

driver:
	cd driver && make

initramfs:
	cd initramfs && ./gen_initramfs.sh

qemu:
	cd qemu && ./run_qemu.sh

quick_turnaround: driver user initramfs qemu