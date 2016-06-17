#
# linux/arch/i386/boot/compressed/Makefile
#
# create a compressed vmlinux image from the original vmlinux
#

HEAD = head.o
SYSTEM = $(TOPDIR)/vmlinux

OBJECTS = $(HEAD) misc.o

IA32_CFLAGS := -O2 -DSTDC_HEADERS

#
# ZIMAGE_OFFSET is the load offset of the compression loader
# BZIMAGE_OFFSET is the load offset of the high loaded compression loader
#
BZIMAGE_OFFSET = 0x100000

BZLINKFLAGS = -Ttext $(BZIMAGE_OFFSET) $(ZLDFLAGS)

all: vmlinux

bvmlinux: piggy.o $(OBJECTS)
	$(IA32_LD) $(BZLINKFLAGS) -o bvmlinux $(OBJECTS) piggy.o

head.o: head.S
	$(IA32_AS) -c head.S

misc.o: misc.c
	$(IA32_CC) $(IA32_CFLAGS) -c misc.c

piggy.o:	$(SYSTEM)
	tmppiggy=_tmp_$$$$piggy; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk; \
	$(OBJCOPY) $(SYSTEM) $$tmppiggy; \
	gzip -f -9 < $$tmppiggy > $$tmppiggy.gz; \
	echo "SECTIONS { .data : { input_len = .; LONG(input_data_end - input_data) input_data = .; *(.data) input_data_end = .; }}" > $$tmppiggy.lnk; \
	$(IA32_LD) -r -o piggy.o -b binary $$tmppiggy.gz -b elf32-i386 -T $$tmppiggy.lnk; \
	rm -f $$tmppiggy $$tmppiggy.gz $$tmppiggy.lnk

clean:
	rm -f vmlinux bvmlinux _tmp_*
