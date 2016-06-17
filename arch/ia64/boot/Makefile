#
# ia64/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1998 by David Mosberger-Tang <davidm@hpl.hp.com>
#

LINKFLAGS = -static -T bootloader.lds

.S.s:
	$(CPP) $(AFLAGS) -traditional -o $*.o $<
.S.o:
	$(CC) $(AFLAGS) -traditional -c -o $*.o $<

OBJECTS	= bootloader.o

targets-$(CONFIG_IA64_HP_SIM) += bootloader
targets-$(CONFIG_IA64_GENERIC) += bootloader

all:	$(targets-y)

bootloader: $(OBJECTS)
	$(LD) $(LINKFLAGS) $(OBJECTS) $(TOPDIR)/lib/lib.a $(TOPDIR)/arch/$(ARCH)/lib/lib.a \
	-o bootloader

clean:
	rm -f $(targets-y) $(OBJECTS)

dep:
