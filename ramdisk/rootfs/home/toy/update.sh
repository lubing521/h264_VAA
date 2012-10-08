#!/bin/sh
tftpd -c /tmp  2> /log.txt
log=`busybox du -k /log.txt|busybox awk '{print $1}'`
uimg=`busybox du -m /tmp/uImage|busybox awk '{print $1}'`
if [ $log -gt 1 ]; then
	rm -rf log.txt
elif [ $uimg -gt 5 ]; then
	rm -rf log.txt                       
	mtd_debug erase /dev/mtd3 0 0x800000 
	nandwrite -p /dev/mtd3 /tmp/uImage 
	reboot 
fi
