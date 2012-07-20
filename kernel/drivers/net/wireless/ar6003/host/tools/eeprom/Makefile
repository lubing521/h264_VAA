#------------------------------------------------------------------------------
# <copyright file="makefile" company="Atheros">
#    Copyright (c) 2005-2008 Atheros Corporation.  All rights reserved.
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
#------------------------------------------------------------------------------
#==============================================================================
# Author(s): ="Atheros"
#==============================================================================
CC := $(ATH_CROSS_COMPILE_TYPE)gcc

# Build a single eeprom binary that works with all
# known targets for which header files are available.

eeprom: eeprom.o AR6002def.o AR6003def.o
	$(CC) -g -Wall -o eeprom eeprom.o AR600?def.o

eeprom.o: eeprom.c
	@$(CC) -g -Wall -I../../include -I../../os/linux/include -I../../../include -c eeprom.c

AR6002def.o:
	@if [ -e ../../../include/AR6002/hw2.0 ]; then \
	    $(CC) -g -Wall -I../../include -I../../os/linux/include -I../../../include -c AR6002def.c; \
	else \
	    echo "SKIP eeprom support for AR6002"; \
	fi

AR6003def.o:
	@if [ -e ../../../include/AR6002/hw4.0 ]; then \
	    $(CC) -g -Wall -I../../include -I../../os/linux/include -I../../../include -c AR6003def.c; \
	else \
	    echo "SKIP eeprom support for AR6003"; \
	fi
	
clean:
	rm -f *.o \
	rm eeprom

	
