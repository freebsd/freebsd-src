#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995, 1998, 2001 by Ralf Baechle
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

#
# Drop some uninteresting sections in the kernel.
# This is only relevant for ELF kernels but doesn't hurt a.out
#
drop-sections	= .reginfo .mdebug
strip-flags	= $(addprefix --remove-section=,$(drop-sections))

all: vmlinux.ecoff vmlinux.srec addinitrd

vmlinux.ecoff:	$(CONFIGURE) elf2ecoff $(TOPDIR)/vmlinux
	./elf2ecoff $(TOPDIR)/vmlinux vmlinux.ecoff $(E2EFLAGS)

elf2ecoff: elf2ecoff.c
	$(HOSTCC) -o $@ $^

vmlinux.srec: $(CONFIGURE) $(TOPDIR)/vmlinux
	$(OBJCOPY) -S -O srec $(strip-flags) $(TOPDIR)/vmlinux vmlinux.srec

addinitrd: addinitrd.c
	$(HOSTCC) -o $@ $^

# Don't build dependencies, this may die if $(CC) isn't gcc
dep:

clean:
	rm -f vmlinux.ecoff
	rm -f vmlinux.srec
	rm -f zImage zImage.tmp

mrproper:
	rm -f vmlinux.ecoff
	rm -f vmlinux.srec
	rm -f addinitrd
	rm -f elf2ecoff

dummy:

include $(TOPDIR)/Rules.make
