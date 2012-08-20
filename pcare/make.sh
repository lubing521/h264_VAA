#!/bin/bash
make distclean
./configure --prefix=/home/cgm/target --host=unicore32-linux CFLAGS="-I../include"
make
cp src/core/pcare ../ramdisk/rootfs/home/toy/
cd ../kernel/
./make.sh
