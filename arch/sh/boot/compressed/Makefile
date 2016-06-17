#
# linux/arch/sh/boot/compressed/Makefile
#
# create a compressed vmlinux image from the original vmlinux
#

HEAD = head.o
SYSTEM = $(TOPDIR)/vmlinux

OBJECTS = $(HEAD) misc.o

ifdef CONFIG_SH_STANDARD_BIOS
OBJECTS += ../../kernel/sh_bios.o
endif

ZLDFLAGS = -e startup -T $(TOPDIR)/arch/sh/vmlinux.lds

#
# ZIMAGE_OFFSET is the load offset of the compression loader
#
ZIMAGE_OFFSET = $(shell printf "0x%8x" $$[0x80000000+0x$(CONFIG_MEMORY_START)+0x$(CONFIG_BOOT_LINK_OFFSET)])

ZLINKFLAGS  = -Ttext $(ZIMAGE_OFFSET)  $(ZLDFLAGS)

all: vmlinux

vmlinux: piggy.o $(OBJECTS)
	$(LD) $(ZLINKFLAGS) -o vmlinux $(OBJECTS) piggy.o

head.o: head.S
	$(CC) $(AFLAGS) -traditional -c head.S

piggy.o:	$(SYSTEM)
	tmppiggy=_tmp_$$$$piggy; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk; \
	$(OBJCOPY) -R .empty_zero_page $(SYSTEM) $$tmppiggy; \
	gzip -f -9 < $$tmppiggy > $$tmppiggy.gz; \
	echo "SECTIONS { .data : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > $$tmppiggy.lnk; \
	$(LD) -r -o piggy.o -b binary $$tmppiggy.gz -b elf32-sh-linux -T $$tmppiggy.lnk; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk

clean:
	rm -f vmlinux _tmp_*
