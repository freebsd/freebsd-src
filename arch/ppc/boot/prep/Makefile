#
# arch/ppc/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Tom Rini	January 2001
#
# Originally:
# arch/ppc/boot/Makefile
# Copyright (C) 1994 by Linus Torvalds
# Adapted for PowerPC by Gary Thomas
# modified by Cort (cort@cs.nmt.edu)
#

USE_STANDARD_AS_RULE		:= true

TFTPIMAGE			= /tftpboot/zImage.prep
ifeq ($(CONFIG_SMP),y)
TFTPIMAGE			:= $(TFTPIMAGE).smp
endif

LD_ARGS				= -T ../ld.script -Ttext 0x00800000 -Bstatic
obj-y				:= head.o ../simple/legacy.o misc.o \
					../common/util.o ../common/string.o \
					../common/misc-common.o \
					../common/serial_stub.o
OBJCOPY_ARGS			= -O elf32-powerpc
LIBS 				= ../lib/zlib.a ../of1275/of1275.a

obj-$(CONFIG_SERIAL_CONSOLE)	+= ../common/ns16550.o
obj-$(CONFIG_VGA_CONSOLE)	+= vreset.o kbd.o

# Tools
MKPREP				:= ../utils/mkprep
SIZE				:= ../utils/size
OFFSET				:= ../utils/offset

# Extra include search dirs
CFLAGS_kbd.o			+= -I$(TOPDIR)/drivers/char

all: zImage

zImage: $(obj-y) $(LIBS) ../ld.script ../images/vmlinux.gz ../common/dummy.o \
		$(MKPREP)
	$(OBJCOPY) $(OBJCOPY_ARGS) -R .comment \
		--add-section=.image=../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		../common/dummy.o image.o
	$(LD) $(LD_ARGS) -o $@ $(obj-y) image.o $(LIBS)
	$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R .comment -R .stab -R .stabstr
	$(MKPREP) -pbp $@ ../images/$@.prep
	rm -f $@

zImage.initrd: $(obj-y) $(LIBS) ../ld.script ../images/vmlinux.gz $(MKPREP) \
		../common/dummy.o
	$(OBJCOPY) $(OBJCOPY_ARGS) -R .comment \
		--add-section=.ramdisk=../images/ramdisk.image.gz \
		--set-section-flags=.ramdisk=contents,alloc,load,readonly,data \
		--add-section=.image=../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data \
		../common/dummy.o image.o
	$(LD) $(LD_ARGS) -o $@ $(obj-y) image.o $(LIBS)
	$(OBJCOPY) $(OBJCOPY_ARGS) $@ $@ -R .comment -R .stab -R .stabstr
	$(MKPREP) -pbp $@ ../images/$@.prep
	rm -f $@

floppy: zImage
	dd if=../images/zImage.prep of=/dev/fd0H1440 bs=64b

znetboot : zImage
	cp ../images/zImage.prep $(TFTPIMAGE)

znetboot.initrd : zImage.initrd
	cp ../images/zImage.initrd.prep $(TFTPIMAGE)

include $(TOPDIR)/Rules.make
