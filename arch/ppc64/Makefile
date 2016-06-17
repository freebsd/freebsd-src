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
# Changes for PPC by Gary Thomas
# Rewritten by Cort Dougan and Paul Mackerras
# Adjusted for PPC64 by Tom Gall
#

KERNELLOAD	=0xc000000000000000

ifeq ($(shell uname -m),ppc64)
CHECKS		= checks
endif

HAS_BIARCH      := $(shell if $(CC) -m64 -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo y; else echo n; fi;)
ifeq ($(HAS_BIARCH),y)
AS              := $(AS) -64
LD              := $(LD) -m elf64ppc
CC		:= $(CC) -m64
endif

LINKFLAGS	= -T arch/ppc64/vmlinux.lds -Bstatic \
		-e $(KERNELLOAD) -Ttext $(KERNELLOAD)
CFLAGS		:= $(CFLAGS) -fsigned-char -msoft-float -pipe \
		-Wno-uninitialized -mminimal-toc -fno-builtin \
		-mtraceback=full
CPP		= $(CC) -E $(CFLAGS)

HAVE_ZERO_BSS := $(shell if $(CC) -fno-zero-initialized-in-bss -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo y; else echo n; fi)

ifeq ($(HAVE_ZERO_BSS),y)
CFLAGS		+= -fno-zero-initialized-in-bss
endif

HEAD := arch/ppc64/kernel/head.o

ARCH_SUBDIRS = arch/ppc64/kernel arch/ppc64/mm arch/ppc64/lib
SUBDIRS := $(SUBDIRS) $(ARCH_SUBDIRS)
ARCHIVES := arch/ppc64/kernel/kernel.o arch/ppc64/mm/mm.o arch/ppc64/lib/lib.o $(ARCHIVES)
CORE_FILES := arch/ppc64/kernel/kernel.o arch/ppc64/mm/mm.o arch/ppc64/lib/lib.o $(CORE_FILES)

ifdef CONFIG_XMON
SUBDIRS += arch/ppc64/xmon
CORE_FILES += arch/ppc64/xmon/x.o
endif

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

checks:
	@$(MAKE) -C arch/$(ARCH)/kernel checks

ifdef CONFIG_PPC_PSERIES
BOOT_TARGETS = zImage znetboot.initrd zImage.initrd
endif

ifdef CONFIG_PPC_ISERIES
BOOT_TARGETS = vmlinux.sminitrd vmlinux.initrd vmlinux.sm
endif

$(BOOT_TARGETS): vmlinux
	@$(MAKEBOOT) $@

znetboot: vmlinux
ifdef CONFIG_SMP
	cp -f vmlinux /tftpboot/vmlinux.smp
else
	cp -f vmlinux /tftpboot/vmlinux
endif
	@$(MAKEBOOT) $@

%_config: arch/ppc64/configs/%_defconfig
	rm -f .config arch/ppc64/defconfig
	cp -f arch/ppc64/configs/$(@:config=defconfig) arch/ppc64/defconfig

archclean:
	rm -f arch/ppc64/kernel/{ppc_defs.h,checks,mk_defs.s,mk_defs_out.c,mk_defs_tpl}
	@$(MAKEBOOT) clean

archmrproper:

archdep:
	$(MAKEBOOT) fastdep
