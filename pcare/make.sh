#!/bin/bash
make distclean
./configure --prefix=/home/cgm/target --host=unicore32-linux CFLAGS="-I../include" LDFLAGS="-L../lib -ljpeg"
make
