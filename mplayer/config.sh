#!/bin/bash

./configure --prefix=/usr/local/mplayer --host-cc=gcc --cc=unicore32-linux-gcc --target=unicore32-linux --disable-win32dll --disable-win32waveout --disable-mencoder --disable-iconv --disable-live --disable-dvdnav --disable-dvdread --disable-dvdread-internal --disable-libdvdcss-internal --disable-ossaudio --enable-mad --disable-mp3lib --disable-fbdev --disable-armv5te --disable-armv6 --disable-ffmpeg_so --disable-md5sum --disable-yuv4mpeg --disable-jpeg --disable-decoder=ffmp3float --disable-dvb --disable-real --extra-cflags="-I/usr/local/mplayer/include -I/usr/local/mplayer/include/alsa" --extra-ldflags=-L/usr/local/mplayer/lib
