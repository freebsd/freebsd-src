#
# alpha/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994 by Linus Torvalds
#

NM := $(NM) -B

LINKFLAGS = -static -T arch/alpha/vmlinux.lds -N #-relax
CFLAGS := $(CFLAGS) -pipe -mno-fp-regs -ffixed-8

# Determine if we can use the BWX instructions with GAS.
old_gas := $(shell if $(AS) --version 2>&1 | grep 'version 2.7' > /dev/null; then echo y; else echo n; fi)

# Determine if GCC understands the -mcpu= option.
have_mcpu := $(shell if $(CC) -mcpu=ev5 -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo y; else echo n; fi)

have_mcpu_pca56 := $(shell if $(CC) -mcpu=pca56 -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo y; else echo n; fi)

have_mcpu_ev6 := $(shell if $(CC) -mcpu=ev6 -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo y; else echo n; fi)

have_mcpu_ev67 := $(shell if $(CC) -mcpu=ev67 -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo y; else echo n; fi)

# Turn on the proper cpu optimizations.
ifeq ($(have_mcpu),y)
  # If GENERIC, make sure to turn off any instruction set extensions that
  # the host compiler might have on by default.  Given that EV4 and EV5
  # have the same instruction set, prefer EV5 because an EV5 schedule is
  # more likely to keep an EV4 processor busy than vice-versa.
  mcpu_done := n
  ifeq ($(CONFIG_ALPHA_GENERIC),y)
    CFLAGS := $(CFLAGS) -mcpu=ev5
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_SX164)$(have_mcpu_pca56),nyy)
    CFLAGS := $(CFLAGS) -mcpu=pca56
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_POLARIS)$(have_mcpu_pca56),nyy)
    CFLAGS := $(CFLAGS) -mcpu=pca56
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_EV4),ny)
    CFLAGS := $(CFLAGS) -mcpu=ev4
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_EV56),ny)
    CFLAGS := $(CFLAGS) -mcpu=ev56
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_EV5),ny)
    CFLAGS := $(CFLAGS) -mcpu=ev5
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_EV67)$(have_mcpu_ev67),nyy)
    CFLAGS := $(CFLAGS) -mcpu=ev67
    mcpu_done := y
  endif
  ifeq ($(mcpu_done)$(CONFIG_ALPHA_EV6),ny)
    ifeq ($(have_mcpu_ev6),y)
      CFLAGS := $(CFLAGS) -mcpu=ev6
    else
      ifeq ($(have_mcpu_pca56),y)
        CFLAGS := $(CFLAGS) -mcpu=pca56
      else
        CFLAGS := $(CFLAGS) -mcpu=ev56
      endif
    endif
    mcpu_done := y
  endif
endif

# For TSUNAMI, we must have the assembler not emulate our instructions.
# The same is true for IRONGATE, POLARIS, PYXIS.
# BWX is most important, but we don't really want any emulation ever.

ifeq ($(old_gas),y)
  # How do we do #error in make?
  CFLAGS := --error-please-upgrade-your-assembler
endif
CFLAGS := $(CFLAGS) -Wa,-mev6

HEAD := arch/alpha/kernel/head.o

SUBDIRS := $(SUBDIRS) arch/alpha/kernel arch/alpha/mm arch/alpha/lib \
	arch/alpha/math-emu
CORE_FILES := arch/alpha/kernel/kernel.o arch/alpha/mm/mm.o $(CORE_FILES)

ifeq ($(CONFIG_MATHEMU),y)
  CORE_FILES := $(CORE_FILES) arch/alpha/math-emu/math-emu.o
endif

LIBS := $(TOPDIR)/arch/alpha/lib/lib.a $(LIBS) $(TOPDIR)/arch/alpha/lib/lib.a

MAKEBOOT = $(MAKE) -C arch/alpha/boot

rawboot:
	@$(MAKEBOOT) rawboot

#
# My boot writes directly to a specific disk partition, I doubt most
# people will want to do that without changes..
#
msb my-special-boot:
	@$(MAKEBOOT) msb

bootimage:
	@$(MAKEBOOT) bootimage

srmboot:
	@$(MAKEBOOT) srmboot

archclean:
	@$(MAKE) -C arch/alpha/kernel clean
	@$(MAKEBOOT) clean
	rm -f arch/alpha/vmlinux.lds

archmrproper:

archdep:
	@$(MAKEBOOT) dep

vmlinux: arch/alpha/vmlinux.lds

arch/alpha/vmlinux.lds: arch/alpha/vmlinux.lds.in
	$(CPP) $(CPPFLAGS) -xc -P arch/alpha/vmlinux.lds.in -o arch/alpha/vmlinux.lds

bootpfile:
	@$(MAKEBOOT) bootpfile

bootpzfile:
	@$(MAKEBOOT) bootpzfile
