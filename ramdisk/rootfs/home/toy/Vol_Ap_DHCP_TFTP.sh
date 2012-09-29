#!/bin/sh

/home/toy/volume_set &
echo "--------Insmod ap ko-----------"
insmod /modules/rtutil3070ap.ko
insmod /modules/rt3070ap.ko
insmod /modules/rtnet3070ap.ko

echo "--------Config ap IP-----------"
ifconfig ra0 192.168.1.100

#echo "----- ---Enable DHCP-----------"
#/sbin/dhcpd -d ra0 &
/sbin/udhcpd &
/sbin/udpsvd -hEv 0 69 /home/toy/update.sh &
