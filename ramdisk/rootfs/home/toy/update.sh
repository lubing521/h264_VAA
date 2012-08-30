#!/bin/sh
tftpd -c /tmp
mtd_debug erase /dev/mtd1 0 0x800000
nandwrite -p /dev/mtd1 /tmp/uImage
echo "New Kernel Updated , Reboot In 5 Seconds .."
echo "5...."
sleep 1
echo "5...."
sleep 1
echo "4...."
sleep 1
echo "3...."
sleep 1
echo "2...."
sleep 1
echo "1...."
sleep 1
reboot
