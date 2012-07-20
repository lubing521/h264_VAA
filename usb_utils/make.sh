#!/bin/bash

cd ./zlib-1.2.3
CC=unicore32-linux-gcc ./configure --prefix="$PWD/../_install"
make
make install

cd ../libusb-0.1.10
./configure CC=unicore32-linux-gcc CPPFLAGS=-I./ --prefix="$PWD/../_install" --host=unicore32-linux
make
make install

cd ../usbutils-0.80
./configure CC=unicore32-linux-gcc --prefix="$PWD/../_install" --host=unicore32-linux LIBUSB_CFLAGS="$PWD/../_install/include/" LIBUSB_LIBS="$PWD/../_install/lib/libusb.so" CPPFLAGS=-I"$PWD/../_install/include/" CFLAGS="-O2"
make
