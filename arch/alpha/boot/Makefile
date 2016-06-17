#
# arch/alpha/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#
# Creating a compressed kernel image (arch/alpha/boot/vmlinux.gz)
# is done via:
#
#	make boot
#
# Creating a BOOTP image (arch/alpha/boot/bootpfile or bootpzfile)
# is done via:
#
#	make bootpfile		# using an uncompressed kernel
#	make bootpzfile		# using a compressed kernel
#
# Adding an INITRD image to a BOOTP file (arch/alpha/boot/bootpzfile)
# can be done, e.g, via:
#
# INITRD=`pwd`/initrd.img make bootpzfile

LINKFLAGS = -static -T bootloader.lds #-N -relax

.S.s:
	$(CPP) $(AFLAGS) -traditional -o $*.o $<
.S.o:
	$(CC) $(AFLAGS) -traditional -c -o $*.o $<

OBJECTS = head.o main.o
BPOBJECTS = head.o bootp.o
BPZOBJECTS = head.o bootpz.o misc.o
TARGETS = vmlinux.gz tools/objstrip # also needed by aboot & milo
VMLINUX = $(TOPDIR)/vmlinux
OBJSTRIP = tools/objstrip

all:	$(TARGETS)
	@echo Ready to install kernel in $(shell pwd)/vmlinux.gz

# normally no need to build these:
rawboot: vmlinux.nh tools/lxboot tools/bootlx

msb:	tools/lxboot tools/bootlx vmlinux.nh
	( cat tools/lxboot tools/bootlx vmlinux.nh ) > /dev/rz0a
	disklabel -rw rz0 'linux' tools/lxboot tools/bootlx

bootimage:	tools/mkbb tools/lxboot tools/bootlx vmlinux.nh
	( cat tools/lxboot tools/bootlx vmlinux.nh ) > bootimage
	tools/mkbb bootimage tools/lxboot

bootpfile:	tools/bootph vmlinux.nh
	cat tools/bootph vmlinux.nh > bootpfile
ifdef INITRD
	cat $(INITRD) >> bootpfile
endif

bootpzfile:	tools/bootpzh vmlinux.nh.gz
	cat tools/bootpzh vmlinux.nh.gz > bootpzfile
ifdef INITRD
	cat $(INITRD) >> bootpzfile
endif

srmboot:	bootdevice bootimage
	dd if=bootimage of=$(BOOTDEV) bs=512 seek=1 skip=1
	tools/mkbb $(BOOTDEV) tools/lxboot

bootdevice:
	@test "$(BOOTDEV)" != ""  || (echo You must specify BOOTDEV ; exit -1)

vmlinux.gz: $(TOPDIR)/vmlinux
	gzip -fv9c $(TOPDIR)/vmlinux > vmlinux.gz

vmlinux.nh.gz: vmlinux.nh
	gzip -fv9c vmlinux.nh > vmlinux.nh.gz

main.o: ksize.h

bootp.o: ksize.h

bootpz.o: kzsize.h

ksize.h: vmlinux.nh dummy
	echo "#define KERNEL_SIZE `ls -l vmlinux.nh | awk '{print $$5}'`" > $@T
ifdef INITRD
	[ ! \( -f $(INITRD) \) ] || exit 1
	echo "#define INITRD_IMAGE_SIZE `ls -l $(INITRD) | awk '{print $$5}'`" >> $@T
endif
	cmp -s $@T $@ || mv -f $@T $@
	rm -f $@T

kzsize.h: vmlinux.nh.gz dummy
	echo "#define KERNEL_SIZE `ls -l vmlinux.nh | awk '{print $$5}'`" > $@T
	echo "#define KERNEL_Z_SIZE `ls -l vmlinux.nh.gz | awk '{print $$5}'`" >> $@T
ifdef INITRD
	[ -f $(INITRD) ] || exit 1
	echo "#define INITRD_IMAGE_SIZE `ls -l $(INITRD) | awk '{print $$5}'`" >> $@T
endif
	cmp -s $@T $@ || mv -f $@T $@
	rm -f $@T

vmlinux.nh: $(VMLINUX) $(OBJSTRIP)
	$(OBJSTRIP) -v $(VMLINUX) vmlinux.nh

vmlinux: $(VMLINUX)
	$(STRIP) -o vmlinux $(VMLINUX)

tools/lxboot: $(OBJSTRIP) bootloader
	$(OBJSTRIP) -p bootloader tools/lxboot

tools/bootlx: bootloader $(OBJSTRIP)
	$(OBJSTRIP) -vb bootloader tools/bootlx

tools/bootph: bootpheader $(OBJSTRIP)
	$(OBJSTRIP) -vb bootpheader tools/bootph

tools/bootpzh: bootpzheader $(OBJSTRIP)
	$(OBJSTRIP) -vb bootpzheader tools/bootpzh

$(OBJSTRIP): $(OBJSTRIP).c
	$(HOSTCC) $(HOSTCFLAGS) -I$(HPATH) $(OBJSTRIP).c -o $(OBJSTRIP)

tools/mkbb: tools/mkbb.c
	$(HOSTCC) tools/mkbb.c -o tools/mkbb

bootloader: $(OBJECTS)
	$(LD) $(LINKFLAGS) $(OBJECTS) $(LIBS) -o bootloader

bootpheader: $(BPOBJECTS)
	$(LD) $(LINKFLAGS) $(BPOBJECTS) $(LIBS) -o bootpheader

bootpzheader: $(BPZOBJECTS)
	$(LD) $(LINKFLAGS) $(BPZOBJECTS) $(LIBS) -o bootpzheader

misc.o: misc.c $(TOPDIR)/lib/inflate.c

clean:
	rm -f $(TARGETS) bootloader bootimage bootpfile bootpheader
	rm -f tools/mkbb tools/bootlx tools/lxboot tools/bootph
	rm -f vmlinux.nh ksize.h

dep:

dummy:
