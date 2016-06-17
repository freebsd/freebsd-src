# $Id: Makefile,v 1.49 2001/07/27 09:42:22 davem Exp $
# sparc/Makefile
#
# Makefile for the architecture dependent flags and dependencies on the
# Sparc.
#
# Copyright (C) 1994 David S. Miller (davem@caip.rutgers.edu)
#

# If the solaris /bin/sh wasn't so broken, I wouldn't need the following
# line...
SHELL  =/bin/bash

#
# Uncomment the first CFLAGS if you are doing kgdb source level
# debugging of the kernel to get the proper debugging information.

IS_EGCS := $(shell if $(CC) -m32 -S -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo y; else echo n; fi; )
NEW_GAS := $(shell if $(LD) --version 2>&1 | grep 'elf64_sparc' > /dev/null; then echo y; else echo n; fi)

ifeq ($(NEW_GAS),y)
AS              := $(AS) -32
LD              := $(LD) -m elf32_sparc
endif

#CFLAGS := $(CFLAGS) -g -pipe -fcall-used-g5 -fcall-used-g7
ifneq ($(IS_EGCS),y)
CFLAGS := $(CFLAGS) -pipe -mno-fpu -fcall-used-g5 -fcall-used-g7
else
CFLAGS := $(CFLAGS) -m32 -pipe -mno-fpu -fcall-used-g5 -fcall-used-g7
endif

#LINKFLAGS = -N -Ttext 0xf0004000
LINKFLAGS = -T arch/sparc/vmlinux.lds

HEAD := arch/sparc/kernel/head.o arch/sparc/kernel/init_task.o

SUBDIRS += arch/sparc/kernel arch/sparc/lib arch/sparc/prom \
	arch/sparc/mm arch/sparc/math-emu

CORE_FILES := arch/sparc/kernel/kernel.o arch/sparc/mm/mm.o $(CORE_FILES) \
	arch/sparc/math-emu/math-emu.o

LIBS := $(TOPDIR)/lib/lib.a $(LIBS) $(TOPDIR)/arch/sparc/prom/promlib.a \
	$(TOPDIR)/arch/sparc/lib/lib.a

# This one has to come last
SUBDIRS += arch/sparc/boot
CORE_FILES_NO_BTFIX := $(CORE_FILES)
CORE_FILES += arch/sparc/boot/btfix.o

export	CORE_FILES_NO_BTFIX

archclean:
	rm -f $(TOPDIR)/vmlinux.aout
	-$(MAKE) -C arch/sparc/boot clean

archmrproper:
	rm -f $(TOPDIR)/include/asm-sparc/asm_offsets.h

archdep: check_asm

check_asm: include/linux/version.h
	$(MAKE) -C arch/sparc/kernel check_asm

tftpboot.img:
	$(MAKE) -C arch/sparc/boot tftpboot.img
