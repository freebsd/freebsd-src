#
# linux/arch/shmedia/boot/compressed/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2002 Stuart Menefy
#
# create a compressed vmlinux image from the original vmlinux
#


# This is a temporary work around for a problem with the Linux ld.
# We can't use the Linux linker, because it doesn't work with binary
# files, so we use the bard machine linker instead...
PIGGYLD=sh64-superh-elf-ld -EL # -m shlelf64
# ...which has the knock on that we need this when doing the final link.
EXTRAFLAGS=--no-warn-mismatch

HEAD = head.o
SYSTEM = $(TOPDIR)/vmlinux

OBJECTS = $(HEAD) misc.o cache.o

ZLDFLAGS = -e startup -T vmlinux.lds

#
# ZIMAGE_OFFSET is the load offset of the compression loader
# (4M for the kernel plus 64K for this loader)
#
ZIMAGE_OFFSET = $(shell printf "0x%8x" $$[0x$(CONFIG_MEMORY_START)+0x400000+0x10000])

ZLINKFLAGS = -Ttext $(ZIMAGE_OFFSET) $(ZLDFLAGS)

all: vmlinux

vmlinux.lds: vmlinux.lds.S
	$(CPP) -traditional -C -P -I$(HPATH) -Ush vmlinux.lds.S >vmlinux.lds

vmlinux: piggy.o $(OBJECTS) vmlinux.lds
	$(LD) $(ZLINKFLAGS) $(EXTRAFLAGS) -o vmlinux $(OBJECTS) piggy.o

head.o: head.S
	$(CC) $(AFLAGS) -traditional -c head.S

piggy.o:	$(SYSTEM)
	tmppiggy=_tmp_$$$$piggy; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk; \
	$(OBJCOPY) -R .empty_zero_page $(SYSTEM) $$tmppiggy; \
	gzip -f -9 < $$tmppiggy > $$tmppiggy.gz; \
	echo "SECTIONS { .image : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > $$tmppiggy.lnk; \
	$(PIGGYLD) -r -o piggy.o -b binary $$tmppiggy.gz -b elf32-sh64 -T $$tmppiggy.lnk; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk

clean:
	rm -f vmlinux _tmp_*
