
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

LD=$(CROSS_COMPILE)ld -m elf64_s390
CPP=$(CC) -E
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -S
LDFLAGS=-e start
ifeq ($(CONFIG_SHARED_KERNEL),y)
LINKFLAGS =-T $(TOPDIR)/arch/s390x/vmlinux-shared.lds $(LDFLAGS)
else
LINKFLAGS =-T $(TOPDIR)/arch/s390x/vmlinux.lds $(LDFLAGS)
endif
MODFLAGS += -fpic

CFLAGS_PIPE := -pipe
CFLAGS_NSR  := -fno-strength-reduce
CFLAGS := $(CFLAGS) $(CFLAGS_PIPE) $(CFLAGS_NSR)

HEAD := arch/s390x/kernel/head.o arch/s390x/kernel/init_task.o

SUBDIRS := $(SUBDIRS) arch/s390x/mm arch/s390x/kernel arch/s390x/lib \
           drivers/s390
CORE_FILES := arch/s390x/mm/mm.o arch/s390x/kernel/kernel.o $(CORE_FILES)
DRIVERS := $(DRIVERS) drivers/s390/io.o
LIBS := $(TOPDIR)/arch/s390x/lib/lib.a $(LIBS) $(TOPDIR)/arch/s390x/lib/lib.a

all: image listing

listing: vmlinux
	@$(MAKEBOOT) listing

arch/s390x/kernel: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/s390x/kernel

arch/s390x/mm: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/s390x/mm

drivers/s390: dummy
	$(MAKE) linuxsubdirs SUBDIRS=drivers/s390

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

image: vmlinux 
	@$(MAKEBOOT) image

install: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=image install

archclean:
	@$(MAKEBOOT) clean
	$(MAKE) -C arch/$(ARCH)/kernel clean

archmrproper:

archdep:
	@$(MAKEBOOT) dep
