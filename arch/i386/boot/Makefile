#
# arch/i386/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#

BOOT_INCL =	$(TOPDIR)/include/linux/config.h \
		$(TOPDIR)/include/linux/autoconf.h \
		$(TOPDIR)/include/asm/boot.h

zImage: $(CONFIGURE) bootsect setup compressed/vmlinux tools/build
	$(OBJCOPY) compressed/vmlinux compressed/vmlinux.out
	tools/build bootsect setup compressed/vmlinux.out $(ROOT_DEV) > zImage

bzImage: $(CONFIGURE) bbootsect bsetup compressed/bvmlinux tools/build
	$(OBJCOPY) compressed/bvmlinux compressed/bvmlinux.out
	tools/build -b bbootsect bsetup compressed/bvmlinux.out $(ROOT_DEV) > bzImage

compressed/vmlinux: $(TOPDIR)/vmlinux
	@$(MAKE) -C compressed vmlinux

compressed/bvmlinux: $(TOPDIR)/vmlinux
	@$(MAKE) -C compressed bvmlinux

zdisk: $(BOOTIMAGE)
	dd bs=8192 if=$(BOOTIMAGE) of=/dev/fd0

zlilo: $(CONFIGURE) $(BOOTIMAGE)
	if [ -f $(INSTALL_PATH)/vmlinuz ]; then mv $(INSTALL_PATH)/vmlinuz $(INSTALL_PATH)/vmlinuz.old; fi
	if [ -f $(INSTALL_PATH)/System.map ]; then mv $(INSTALL_PATH)/System.map $(INSTALL_PATH)/System.old; fi
	cat $(BOOTIMAGE) > $(INSTALL_PATH)/vmlinuz
	cp $(TOPDIR)/System.map $(INSTALL_PATH)/
	if [ -x /sbin/lilo ]; then /sbin/lilo; else /etc/lilo/install; fi

install: $(CONFIGURE) $(BOOTIMAGE)
	sh -x ./install.sh $(KERNELRELEASE) $(BOOTIMAGE) $(TOPDIR)/System.map "$(INSTALL_PATH)"

tools/build: tools/build.c
	$(HOSTCC) $(HOSTCFLAGS) -o $@ $< -I$(TOPDIR)/include

bootsect: bootsect.o
	$(LD) -Ttext 0x0 -s --oformat binary -o $@ $<

bootsect.o: bootsect.s
	$(AS) -o $@ $<

bootsect.s: bootsect.S Makefile $(BOOT_INCL)
	$(CPP) $(CPPFLAGS) -traditional $(SVGA_MODE) $(RAMDISK) $< -o $@

bbootsect: bbootsect.o
	$(LD) -Ttext 0x0 -s --oformat binary $< -o $@

bbootsect.o: bbootsect.s
	$(AS) -o $@ $<

bbootsect.s: bootsect.S Makefile $(BOOT_INCL)
	$(CPP) $(CPPFLAGS) -D__BIG_KERNEL__ -traditional $(SVGA_MODE) $(RAMDISK) $< -o $@

setup: setup.o
	$(LD) -Ttext 0x0 -s --oformat binary -e begtext -o $@ $<

setup.o: setup.s
	$(AS) -o $@ $<

setup.s: setup.S video.S Makefile $(BOOT_INCL) $(TOPDIR)/include/linux/version.h $(TOPDIR)/include/linux/compile.h
	$(CPP) $(CPPFLAGS) -D__ASSEMBLY__ -traditional $(SVGA_MODE) $(RAMDISK) $< -o $@

bsetup: bsetup.o
	$(LD) -Ttext 0x0 -s --oformat binary -e begtext -o $@ $<

bsetup.o: bsetup.s
	$(AS) -o $@ $<

bsetup.s: setup.S video.S Makefile $(BOOT_INCL) $(TOPDIR)/include/linux/version.h $(TOPDIR)/include/linux/compile.h
	$(CPP) $(CPPFLAGS) -D__BIG_KERNEL__ -D__ASSEMBLY__ -traditional $(SVGA_MODE) $(RAMDISK) $< -o $@

dep:

clean:
	rm -f tools/build
	rm -f setup bootsect zImage compressed/vmlinux.out
	rm -f bsetup bbootsect bzImage compressed/bvmlinux.out
	@$(MAKE) -C compressed clean
