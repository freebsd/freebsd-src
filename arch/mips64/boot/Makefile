#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995, 1998, 1999 by Ralf Baechle
#

USE_STANDARD_AS_RULE := true

#
# Some DECstations need all possible sections of an ECOFF executable
#
ifdef CONFIG_DECSTATION
  E2EFLAGS = -a
else
  E2EFLAGS =
endif

all: vmlinux.ecoff addinitrd

vmlinux.ecoff:	$(CONFIGURE) elf2ecoff $(TOPDIR)/vmlinux
	./elf2ecoff $(TOPDIR)/vmlinux vmlinux.ecoff $(E2EFLAGS)

elf2ecoff: $(TOPDIR)/arch/mips/boot/elf2ecoff.c
	$(HOSTCC) -I$(TOPDIR)/arch/mips/boot -I- -o $@ $^

addinitrd: $(TOPDIR)/arch/mips/boot/addinitrd.c
	$(HOSTCC) -I$(TOPDIR)/arch/mips/boot -I- -o $@ $^

# Don't build dependencies, this may die if $(CC) isn't gcc
dep:

clean:
	rm -f vmlinux.ecoff

mrproper:
	rm -f vmlinux.ecoff addinitrd elf2ecoff

dummy:

include $(TOPDIR)/Rules.make
