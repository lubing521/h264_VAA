#!/bin/bash
make distclean
./configure CFLAGS="-I../include" #--host=unicore32-linux --prefix=/home/cgm/target 
make
cp src/core/pcare ../ramdisk/rootfs/home/toy/
#cd ../kernel/
#./make.sh
