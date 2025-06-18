#!/bin/sh
set -e

BUSYBOX_VERSION=1.36.1
BUILD_DIR=$(pwd)
ROOTFS_DIR="$BUILD_DIR/rootfs"
BUSYBOX_DIR="$BUILD_DIR/busybox-$BUSYBOX_VERSION"

# Download and extract busybox
if [ ! -d "$BUSYBOX_DIR" ]; then
    if [ ! -f busybox-$BUSYBOX_VERSION.tar.bz2 ]; then
        wget https://busybox.net/downloads/busybox-$BUSYBOX_VERSION.tar.bz2
    fi
    tar -xf busybox-$BUSYBOX_VERSION.tar.bz2
fi

# Build busybox only if not already built
if [ ! -x "$BUSYBOX_DIR/busybox" ]; then
    cd "$BUSYBOX_DIR"
    make defconfig
    sed -i 's/^CONFIG_INSTALL_APPLET_SYMLINKS=y/# CONFIG_INSTALL_APPLET_SYMLINKS is not set/' .config
    sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
    echo 'CONFIG_INSTALL_APPLET_HARDLINKS=y' >> .config
    echo 'CONFIG_FEATURE_INSTALLER=y' >> .config
    sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/' .config
    make oldconfig
    make -j"$(nproc)"

    # Install to rootfs
    cd "$BUSYBOX_DIR"
    make CONFIG_PREFIX="$ROOTFS_DIR" install
fi


# Prepare rootfs
cd "$ROOTFS_DIR"
mkdir -p proc sys dev etc
cp "$BUILD_DIR/init" .
cp "$BUILD_DIR/../driver/edu_drv.ko" .
cp "$BUILD_DIR/../user/edu_cli" .

# Make init executable
chmod +x init

# gzip to initramfs.cpio.gz
find . | cpio -o -H newc --owner=0:0 | gzip > "$BUILD_DIR/../initramfs.cpio.gz"
