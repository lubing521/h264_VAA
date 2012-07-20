#------------------------------------------------------------------------------
# <copyright file="makefile" company="Atheros">
#    Copyright (c) 2010 Atheros Corporation.  All rights reserved.
#
# 
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
#
#
# </copyright>
# 
# <summary>
# 	WAC (Wireless Auto Configuration)
# </summary>
#
#------------------------------------------------------------------------------
#==============================================================================
# Author(s): ="Atheros"
#==============================================================================
CC := $(ATH_CROSS_COMPILE_TYPE)gcc
AR = $(ATH_CROSS_COMPILE_TYPE)ar
RANLIB = $(ATH_CROSS_COMPILE_TYPE)ranlib

KERNEL_SRC :=$(ATH_LINUXPATH)

BUILD_STATIC = y

# BUILD_STRIPPING = y

ifndef CFLAGS
CFLAGS = -Os -W -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow \
	-Wpointer-arith -Wcast-qual
endif

DEPFLAGS=-MMD

CFLAGS += -DCONFIG_CTRL_IFACE_UNIX -DCONFIG_CTRL_IFACE -DWAPI_ENABLE -DWAC_ENABLE

#WPA_SUPPLICANT_IN_USE_PATH=opensrc_0_6_9

CFLAGS += -I./ -I$(KERNEL_SRC)/include -I../../include -I../../../include -I../../wlan/include \
	    -I../../os/linux/include -I../../3rdparty/supplicant/$(WPA_SUPPLICANT_IN_USE_PATH)/src/common \
	    -I../../3rdparty/supplicant/$(WPA_SUPPLICANT_IN_USE_PATH)/src/utils

STATIC=libwac.a
DYNAMIC=libwac.so

PICFLAG=-fPIC

OBJS_LIB = wac_lib.o wac_utils.o

ifdef BUILD_STATIC
 WACLIB=$(STATIC)
else
 WACLIB=$(DYNAMIC)
endif

ifdef BUILD_STRIPPING
  STRIPFLAGS= -Wl,-s
else
  STRIPFLAGS=
endif


RM = rm -f

LIBS = -lpthread

XCFLAGS=$(CFLAGS) $(DEPFLAGS)

all: $(WACLIB) wac_app

%: %.o
	$(CC) $(LDFLAGS) $(STRIPFLAGS) $(XCFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(XCFLAGS) -c $<

%.so: %.c
	$(CC) $(XCFLAGS) $(PICFLAG) -c -o $@ $<

wac_app: wac_app.o $(WACLIB)
	$(CC) $(LDFLAGS) $(STRIPFLAGS) $(XCFLAGS) -o $@ $^ $(LIBS) $(WACLIB)

$(DYNAMIC): $(OBJS_LIB:.o=.so)
	$(CC) -shared -o $@ -Wl,-soname,$@ $(STRIPFLAGS) $(LIBS) -lc $^

$(STATIC): $(OBJS_LIB:.o=.so)
	$(RM) $@
	$(AR) cru $@ $^
#	$(RANLIB) $@
	
clean:
	$(RM) wac_app
	$(RM) *.o *.d *.so *.a
