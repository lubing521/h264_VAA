#! /bin/sh
PWD=`pwd`
export PATH=$PATH:${PWD}/install/bin
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${PWD}/lib:${PWD}/install/lib
if [ ! -d ${PWD}/var ];then
	echo "Creating ${PWD}/var"
	mkdir ${PWD}/var
fi

#if [ -d /var ];then
rm -R /var
echo "ln /var"
ln -s ${PWD}/var /var
#fi

if [ ! -f /usr/bin/which -o ! -f /usr/bin/dirname ];then
mkdir -p /usr/bin
ln -s /bin/busybox /usr/bin/which
ln -s /bin/busybox /usr/bin/dirname
fi

echo "Creating libs"
mkdir /lib -p
for i in `ls lib`;do 
if [ ! -f /lib/${i} ]; then
 echo "/lib/${i}===>${PWD}/lib/${i}"
 ln -s ${PWD}/lib/${i} /lib/${i}
fi
done

mkdir /etc/ -p

echo "nodev oprofilefs /dev/oprofile rw 0 0" > /etc/mtab

mkdir /dev/oprofile
mount | grep oprofilefs
if [ $? -ne 0 ] ;then
mount -t oprofilefs oprofilefs /dev/oprofile
fi

opcontrol --setup --callgraph=2 --session-dir=/data/first --vmlinux=/vmlinux

echo "Setting up completed"


