#!/bin/bash
set -e
KERNEL_VERSION=$(uname -r | cut -d '-' -f1)
JOBS=$(nproc)
BUILD_DIR=$(pwd)

# Download kernel
if [ ! -d linux-$KERNEL_VERSION ]; then
    wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-$KERNEL_VERSION.tar.xz
    tar -xf linux-$KERNEL_VERSION.tar.xz
    ln -s linux-$KERNEL_VERSION linux
fi

cd linux-$KERNEL_VERSION

# Configuration
make defconfig
scripts/config --enable CONFIG_DEVTMPFS
scripts/config --enable CONFIG_DEVTMPFS_MOUNT
scripts/config --enable CONFIG_PCI
scripts/config --enable CONFIG_EXT4_FS
scripts/config --module CONFIG_EDAC
make -j$(nproc) CC="gcc -std=gnu99" modules_prepare
make -j$(nproc) CC="gcc -std=gnu99"

cp arch/x86/boot/bzImage "$BUILD_DIR/../bzImage"