bin_PROGRAMS=pcare
pcare_SOURCES=main.c utils.c rw.c
SUBDIRS=../audio ../video ../network ../tem_rh ../control  
#../leds
INCLUDES=-I$(top_srcdir)/src/include
LDADD = $(top_srcdir)/src/video/libvideo.a $(top_srcdir)/src/network/libnetwork.a $(top_srcdir)/src/audio/libaudio.a $(top_srcdir)/src/control/libcontrol.a $(top_srcdir)/src/lib/libjpeg.a $(top_srcdir)/src/lib/libmad.so.0.2.1
