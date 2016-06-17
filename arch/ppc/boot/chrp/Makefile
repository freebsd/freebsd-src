#
# Makefile for making ELF bootable images for booting on CHRP
# using Open Firmware.
#
# Geert Uytterhoeven	September 1997
#
# Based on coffboot by Paul Mackerras

USE_STANDARD_AS_RULE := true

LD_ARGS = -T ../ld.script -Ttext 0x00400000

OBJS = ../common/crt0.o start.o main.o misc.o ../common/string.o image.o \
	../common/ofcommon.o
LIBS = $(TOPDIR)/lib/lib.a ../lib/zlib.a ../of1275/of1275.a
ADDNOTE = ../utils/addnote
PIGGYBACK = ../utils/piggyback

ifeq ($(CONFIG_PPC64BRIDGE),y)
END += .64
AFLAGS += -Wa,-mppc64bridge
endif
ifeq ($(CONFIG_SMP),y)
END += .smp
endif

TFTPIMAGE=/tftpboot/zImage.chrp$(END)

AFLAGS_../common/crt0.o		+= -I$(TOPDIR)/arch/$(ARCH)/kernel

all: zImage

znetboot: zImage
	cp -f $(TOPDIR)/vmlinux /tftpboot/vmlinux$(END)
	cp ../images/zImage.chrp $(TFTPIMAGE)

znetboot.initrd: zImage.initrd
	cp ../images/zImage.initrd.chrp $(TFTPIMAGE)

floppy: zImage
	mcopy zImage a:zImage

image.o: ../images/vmlinux.gz ../common/dummy.o
	$(OBJCOPY) ../common/dummy.o $@ \
		--add-section=.image=../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data
ifdef CONFIG_XMON
	$(OBJCOPY) $@ $@ \
		--add-section=.sysmap=$(TOPDIR)/System.map \
		--set-section-flags=.sysmap=contents,alloc,load,readonly,data
endif

zImage: $(OBJS) $(LIBS) $(ADDNOTE) ../ld.script
	$(LD) $(LD_ARGS) -o ../images/$@.chrp $(OBJS) $(LIBS)
	$(OBJCOPY) ../images/$@.chrp ../images/$@.chrp -R .comment -R .ramdisk
	cp ../images/$@.chrp ../images/$@.chrp-rs6k
	$(ADDNOTE) ../images/$@.chrp-rs6k

zImage.initrd: $(OBJS) $(LIBS) $(ADDNOTE) ../images/ramdisk.image.gz ../ld.script
	$(OBJCOPY) image.o image.o \
		--add-section=.ramdisk=../images/ramdisk.image.gz \
		--set-section-flags=.ramdisk=contents,alloc,load,readonly,data
	$(LD) $(LD_ARGS) -o ../images/$@.chrp $(OBJS) $(LIBS)
	$(OBJCOPY) ../images/$@.chrp ../images/$@.chrp -R .comment
	cp ../images/$@.chrp ../images/$@.chrp-rs6k
	$(ADDNOTE) ../images/$@.chrp-rs6k

include $(TOPDIR)/Rules.make
