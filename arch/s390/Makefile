
# s390/Makefile
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#

LD=$(CROSS_COMPILE)ld -m elf_s390
CPP=$(CC) -E
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -S
LDFLAGS=-e start
ifeq ($(CONFIG_SHARED_KERNEL),y)
LINKFLAGS =-T $(TOPDIR)/arch/s390/vmlinux-shared.lds $(LDFLAGS)
else
LINKFLAGS =-T $(TOPDIR)/arch/s390/vmlinux.lds $(LDFLAGS)
endif

CFLAGS_PIPE := -pipe
CFLAGS_NSR  := -fno-strength-reduce
CFLAGS := $(CFLAGS) $(CFLAGS_PIPE) $(CFLAGS_NSR)

HEAD := arch/s390/kernel/head.o arch/s390/kernel/init_task.o

SUBDIRS := $(SUBDIRS) arch/s390/mm arch/s390/kernel arch/s390/lib \
           drivers/s390 arch/s390/math-emu
CORE_FILES := arch/s390/mm/mm.o arch/s390/kernel/kernel.o $(CORE_FILES)
DRIVERS := $(DRIVERS) drivers/s390/io.o
LIBS := $(TOPDIR)/arch/s390/lib/lib.a $(LIBS) $(TOPDIR)/arch/s390/lib/lib.a

ifeq ($(CONFIG_MATHEMU),y)
  CORE_FILES := $(CORE_FILES) arch/s390/math-emu/math-emu.o
endif

all: image listing

listing: vmlinux
	@$(MAKEBOOT) listing

arch/s390/kernel: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/s390/kernel

arch/s390/mm: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/s390/mm

drivers/s390: dummy
	$(MAKE) linuxsubdirs SUBDIRS=drivers/s390

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

image: vmlinux 
	@$(MAKEBOOT) image

archclean:
	@$(MAKEBOOT) clean
	$(MAKE) -C arch/$(ARCH)/kernel clean

archmrproper:

archdep:
	@$(MAKEBOOT) dep

install: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=image install
