#!/bin/sh

echo "--------Insmod ap ko-----------"
insmod /modules/rtutil3070ap.ko
insmod /modules/rt3070ap.ko
insmod /modules/rtnet3070ap.ko

echo "--------Config ap IP-----------"
ifconfig ra0 192.168.100.100

#echo "----- ---Enable DHCP-----------"
#/sbin/dhcpd -d ra0 &
/sbin/udhcpd &
