#!/bin/sh
tftpd -c /tmp
mtd_debug erase /dev/mtd1 0 0x800000
nandwrite -p /dev/mtd1 /tmp/uImage
reboot
