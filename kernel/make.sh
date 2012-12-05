#!/bin/bash
rm arch/unicore/boot/uImage 
cd usr
rm built-in.o .built-in.o.cmd .gen_init_cpio.cmd initramfs_data.cpio .initramfs_data.cpio.cmd .initramfs_data.cpio.d initramfs_data.o .initramfs_data.o.cmd
cd ..
make pcare_defconfig
make uImage -j4
cd drivers/wifi-ap/
./make.sh
cd ../../
make uImage -j4
#cp arch/unicore/boot/uImage ../pcare/
