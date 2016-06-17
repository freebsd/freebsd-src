#
# arch/ppc/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
# Adapted for PowerPC by Gary Thomas
# modified by Cort (cort@cs.nmt.edu)
#

GZIP_FLAGS = -v9f

CFLAGS	+= -fno-builtin -D__BOOTER__ -I$(TOPDIR)/arch/$(ARCH)/boot/include

AFLAGS	+= -D__BOOTER__
OBJCOPY_ARGS = -O elf32-powerpc

MKIMAGE				:= $(TOPDIR)/scripts/mkuboot.sh

lib/zlib.a: lib/zlib.c
	$(MAKE) -C lib

images/vmlinux.gz: $(TOPDIR)/vmlinux
	$(MAKE) -C images vmlinux.gz

# Subdirs and tools needed for each.  Assume we always need to go into
# 'simple' unless told otherwise.
subdir-y			:= lib common simple
subdir-$(CONFIG_ALL_PPC)	:= chrp pmac prep
tools-$(CONFIG_ALL_PPC)		:= addnote mknote hack-coff mkprep
tools-$(CONFIG_4xx)		:= mktree
tools-$(CONFIG_LOPEC)		:= mkbugboot mkprep
tools-$(CONFIG_PPLUS)		:= mkbugboot mkprep
tools-$(CONFIG_PRPMC750)	:= mkbugboot mkprep
tools-$(CONFIG_SPRUCE)		:= mktree

# These are dirs we don't want to go into on BOOT_TARGETS.  We have them for
# the 'depend' stage.
NONBOOT				:= lib common

# These are the subdirs we want to use
BOOTDIRS			= $(filter-out $(NONBOOT), $(subdir-y))

makeof1275:
	$(MAKE) -C of1275

# This will make the tools we need.  We do it like this to ensure that we use
# HOSTCC. -- Tom
maketools:
	$(MAKE) -C utils $(tools-y)

# The targets all boards support for boot images.
BOOT_TARGETS = zImage zImage.initrd znetboot znetboot.initrd

$(BOOT_TARGETS): vmapus lib/zlib.a images/vmlinux.gz makeof1275 maketools
ifneq ($(BOOTDIRS),)
	for d in $(BOOTDIRS); do $(MAKE) -C $$d $@; done
endif

vmapus: $(TOPDIR)/vmlinux
ifdef CONFIG_APUS
	$(STRIP) $(TOPDIR)/vmlinux -o images/vmapus
	gzip $(GZIP_FLAGS) images/vmapus
endif

# Make an image for PPCBoot / U-Boot.
uImage: $(MKIMAGE) images/vmlinux.gz
	$(CONFIG_SHELL) $(MKIMAGE) -A ppc -O linux -T kernel \
	-C gzip -a 00000000 -e 00000000 \
	-n 'Linux-$(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION)' \
	-d images/vmlinux.gz images/vmlinux.UBoot
	ln -sf vmlinux.UBoot images/uImage
	rm -f ./mkuboot

# These are subdirs with files not normally rm'ed. -- Tom
clean:
	$(MAKE) -C images clean
	$(MAKE) -C utils clean

include $(TOPDIR)/Rules.make
