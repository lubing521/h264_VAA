#!/bin/sh
unicore32-linux-gcc -o t_rh t_rh.c
chmod 777 t_rh
cp t_rh /home/jerry/work/qihuan/ramdisk/rootfs/home/toy

