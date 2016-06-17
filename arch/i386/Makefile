#
# i386/Makefile
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
# 19990713  Artur Skawina <skawina@geocities.com>
#           Added '-march' and '-mpreferred-stack-boundary' support
#

LD=$(CROSS_COMPILE)ld -m elf_i386
OBJCOPY=$(CROSS_COMPILE)objcopy -O binary -R .note -R .comment -S
LDFLAGS=-e stext
LINKFLAGS =-T $(TOPDIR)/arch/i386/vmlinux.lds $(LDFLAGS)

CFLAGS += -pipe

check_gcc = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi)

# prevent gcc from keeping the stack 16 byte aligned
CFLAGS += $(call check_gcc,-mpreferred-stack-boundary=2,)

ifdef CONFIG_M386
CFLAGS += -march=i386
endif

ifdef CONFIG_M486
CFLAGS += -march=i486
endif

ifdef CONFIG_M586
CFLAGS += -march=i586
endif

ifdef CONFIG_M586TSC
CFLAGS += -march=i586
endif

ifdef CONFIG_M586MMX
CFLAGS += -march=i586
endif

ifdef CONFIG_M686
CFLAGS += -march=i686
endif

ifdef CONFIG_MPENTIUMIII
CFLAGS += -march=i686
endif

ifdef CONFIG_MPENTIUM4
CFLAGS += -march=i686
endif

ifdef CONFIG_MK6
CFLAGS += $(call check_gcc,-march=k6,-march=i586)
endif

ifdef CONFIG_MK7
CFLAGS += $(call check_gcc,-march=athlon,-march=i686 -malign-functions=4)
endif

ifdef CONFIG_MCRUSOE
CFLAGS += -march=i686
CFLAGS += $(call check_gcc,-falign-functions=0 -falign-jumps=0 -falign-loops=0,-malign-functions=0 -malign-jumps=0 -malign-loops=0)
endif

ifdef CONFIG_MWINCHIPC6
CFLAGS += -march=i586
endif

ifdef CONFIG_MWINCHIP2
CFLAGS += -march=i586
endif

ifdef CONFIG_MWINCHIP3D
CFLAGS += -march=i586
endif

ifdef CONFIG_MCYRIXIII
CFLAGS += $(call check_gcc,-march=c3,-march=i486)
CFLAGS += $(call check_gcc,-falign-functions=0 -falign-jumps=0 -falign-loops=0,-malign-functions=0 -malign-jumps=0 -malign-loops=0)
endif

ifdef CONFIG_MVIAC3_2
CFLAGS += $(call check_gcc,-march=c3-2,-march=i686)
endif

HEAD := arch/i386/kernel/head.o arch/i386/kernel/init_task.o

SUBDIRS += arch/i386/kernel arch/i386/mm arch/i386/lib

CORE_FILES := arch/i386/kernel/kernel.o arch/i386/mm/mm.o $(CORE_FILES)
LIBS := $(TOPDIR)/arch/i386/lib/lib.a $(LIBS) $(TOPDIR)/arch/i386/lib/lib.a

ifdef CONFIG_MATH_EMULATION
SUBDIRS += arch/i386/math-emu
DRIVERS += arch/i386/math-emu/math.o
endif

arch/i386/kernel: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/i386/kernel

arch/i386/mm: dummy
	$(MAKE) linuxsubdirs SUBDIRS=arch/i386/mm

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

vmlinux: arch/i386/vmlinux.lds

FORCE: ;

.PHONY: zImage bzImage compressed zlilo bzlilo zdisk bzdisk install \
		clean archclean archmrproper archdep

zImage: vmlinux
	@$(MAKEBOOT) zImage

bzImage: vmlinux
	@$(MAKEBOOT) bzImage

compressed: zImage

zlilo: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=zImage zlilo

tmp:
	@$(MAKEBOOT) BOOTIMAGE=bzImage zlilo
bzlilo: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=bzImage zlilo

zdisk: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=zImage zdisk

bzdisk: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=bzImage zdisk

install: vmlinux
	@$(MAKEBOOT) BOOTIMAGE=bzImage install

archclean:
	@$(MAKEBOOT) clean

archmrproper:

archdep:
	@$(MAKEBOOT) dep
