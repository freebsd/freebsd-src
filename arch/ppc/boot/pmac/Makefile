#
# Makefile for making XCOFF bootable images for booting on PowerMacs
# using Open Firmware.
#
# Paul Mackerras	January 1997
#
# Cleaned up, moved into arch/ppc/boot/pmac
# Tom Rini		January 2001

OBJCOPY_ARGS = -O aixcoff-rs6000 -R .stab -R .stabstr -R .comment
COFF_LD_ARGS = -T ../ld.script -e _start -Ttext 0x00500000 -Bstatic
CHRP_LD_ARGS = -T ../ld.script -Ttext 0x01000000

COMMONOBJS = start.o misc.o ../common/string.o ../common/ofcommon.o
COFFOBJS = ../common/coffcrt0.o $(COMMONOBJS) coffmain.o
CHRPOBJS = ../common/crt0.o $(COMMONOBJS) chrpmain.o
LIBS = $(TOPDIR)/lib/lib.a ../lib/zlib.a ../of1275/of1275.a

MKNOTE := ../utils/mknote
SIZE := ../utils/size
OFFSET := ../utils/offset
HACKCOFF := ../utils/hack-coff

ifdef CONFIG_SMP
END := .smp
endif
ifdef CONFIG_PPC64BRIDGE
END += .64
endif

TFTPIMAGE=/tftpboot/zImage.pmac$(END)

../common/coffcrt0.o:
	$(MAKE) -C ../common coffcrt0.o

image.o: ../images/vmlinux.gz ../common/dummy.o
	$(OBJCOPY) ../common/dummy.o $@ -R .comment \
		--add-section=.image=../images/vmlinux.gz \
		--set-section-flags=.image=contents,alloc,load,readonly,data
ifdef CONFIG_XMON
	$(OBJCOPY) $@ $@ \
		--add-section=.sysmap=$(TOPDIR)/System.map \
		--set-section-flags=.sysmap=contents,alloc,load,readonly,data
endif

znetboot: vmlinux.coff vmlinux.elf-pmac zImage
	cp ../images/vmlinux.coff $(TFTPIMAGE)
	cp ../images/vmlinux.elf-pmac $(TFTPIMAGE).elf

znetboot.initrd: vmlinux.initrd.coff vmlinux.initrd.elf-pmac
	cp ../images/vmlinux.initrd.coff $(TFTPIMAGE)
	cp ../images/vmlinux.initrd.elf-pmac $(TFTPIMAGE).elf

miboot.image: ../common/dummy.o ../images/vmlinux.gz
	$(OBJCOPY) $(OBJCOPY_ARGS) --add-section=image=../images/vmlinux.gz \
		../common/dummy.o ../images/$@

miboot.initrd.image: miboot.image ../images/ramdisk.image.gz
	$(OBJCOPY) $(OBJCOPY_ARGS) --add-section=initrd=../images/ramdisk.image.gz \
		../images/miboot.image ../images/$@

coffboot: $(COFFOBJS) image.o $(LIBS) ../ld.script
	$(LD) -o $@ $(COFF_LD_ARGS) $(COFFOBJS) image.o $(LIBS)
	$(OBJCOPY) $@ $@ -R .comment

coffboot.initrd: $(COFFOBJS) image.o $(LIBS) ../ld.script \
		../images/ramdisk.image.gz
	$(OBJCOPY) image.o image-coff.o \
		--add-section=.ramdisk=../images/ramdisk.image.gz \
		--set-section-flags=.ramdisk=contents,alloc,load,readonly,data
	$(LD) -o $@ $(COFF_LD_ARGS) $(COFFOBJS) image-coff.o $(LIBS)
	$(OBJCOPY) $@ $@ -R .comment
	rm -f image-coff.o

vmlinux.coff: coffboot $(HACKCOFF)
	$(OBJCOPY) $(OBJCOPY_ARGS) coffboot ../images/$@
	$(HACKCOFF) ../images/$@
	rm -f coffboot
	ln -sf vmlinux.coff ../images/zImage.pmac

vmlinux.initrd.coff: coffboot.initrd $(HACKCOFF)
	$(OBJCOPY) $(OBJCOPY_ARGS) coffboot.initrd ../images/$@
	$(HACKCOFF) ../images/$@
	rm -f coffboot.initrd
	ln -sf vmlinux.initrd.coff ../images/zImage.initrd.pmac

vmlinux.elf-pmac: $(CHRPOBJS) $(LIBS) $(MKNOTE) image.o ../ld.script
	$(LD) $(CHRP_LD_ARGS) -o ../images/$@ $(CHRPOBJS) $(LIBS) image.o
	$(MKNOTE) > note
	$(OBJCOPY) ../images/$@ ../images/$@ --add-section=.note=note \
		-R .comment -R .ramdisk
	rm -f note

vmlinux.initrd.elf-pmac: $(CHRPOBJS) $(LIBS) $(MKNOTE) image.o \
		../images/ramdisk.image.gz ../ld.script
	$(OBJCOPY) image.o image-elf.o \
		--add-section=.ramdisk=../images/ramdisk.image.gz \
		--set-section-flags=.ramdisk=contents,alloc,load,readonly,data
	$(LD) $(CHRP_LD_ARGS) -o ../images/$@ $(CHRPOBJS) $(LIBS) image-elf.o
	$(MKNOTE) > note
	$(OBJCOPY) ../images/$@ ../images/$@ --add-section=.note=note \
		-R .comment
	rm -f note image-elf.o

zImage: vmlinux.coff vmlinux.elf-pmac miboot.image

zImage.initrd: vmlinux.initrd.coff vmlinux.initrd.elf-pmac miboot.initrd.image

include $(TOPDIR)/Rules.make
