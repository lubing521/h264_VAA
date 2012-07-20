#!/bin/bash

# Housekeeping...clean before img
rm -f /tmp/ramdisk.img

# Ramdisk Constants
# 80M = 80 * 1024 * 1024
#RDSIZE=8192
RDSIZE=16384 
BLKSIZE=1024

# Create an empty ramdisk image
dd if=/dev/zero of=/tmp/ramdisk.img bs=$BLKSIZE count=$RDSIZE

# Make it an ext2 mountable file system
/sbin/mke2fs -F -m 0 -b $BLKSIZE /tmp/ramdisk.img $RDSIZE

# Mount it so that we can populate
mkdir -p /mnt/ramdisk
mount /tmp/ramdisk.img /mnt/ramdisk -t ext2 -o loop

# Populate the filesystem (subdirectories)
cp -ar ./rootfs/* /mnt/ramdisk/

mkdir -p /mnt/ramdisk/sys
mkdir -p /mnt/ramdisk/proc
mkdir -p /mnt/ramdisk/var
mkdir -p /mnt/ramdisk/tmp
mkdir -p /mnt/ramdisk/mnt
mkdir -p /mnt/ramdisk/dev

# Finish up...
umount /mnt/ramdisk
cp /tmp/ramdisk.img ./
rm -fr /mnt/ramdisk
