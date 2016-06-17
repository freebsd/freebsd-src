#
# arch/arm/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995-2001 by Russell King

LINKFLAGS	:=-p -X -T arch/arm/vmlinux.lds
OBJCOPYFLAGS	:=-O binary -R .note -R .comment -S
GZFLAGS		:=-9
CFLAGS		+=-Uarm -fno-common -pipe

ifeq ($(CONFIG_FRAME_POINTER),y)
CFLAGS		:=$(CFLAGS:-fomit-frame-pointer=-mapcs -mno-sched-prolog)
endif

CFLAGS		:=$(CFLAGS:-O2=-Os)

ifeq ($(CONFIG_DEBUG_INFO),y)
CFLAGS		+=-g
endif

# Select CPU dependent flags.  Note that order of declaration is important;
# the options further down the list override previous items.
#
# Note!  For APCS-26 YOU MUST HAVE AN APCS-26 LIBGCC.A
#
apcs-y				:=-mapcs-32
apcs-$(CONFIG_CPU_26)		:=-mapcs-26 -mcpu=arm3 -Os

# This selects which instruction set is used.
# Note that GCC is lame - it doesn't numerically define an
# architecture version macro, but instead defines a whole
# series of macros.
arch-y				:=
arch-$(CONFIG_CPU_32v3)		:=-D__LINUX_ARM_ARCH__=3 -march=armv3
arch-$(CONFIG_CPU_32v4)		:=-D__LINUX_ARM_ARCH__=4 -march=armv4
arch-$(CONFIG_CPU_32v5)		:=-D__LINUX_ARM_ARCH__=5 -march=armv5

# This selects how we optimise for the processor.
tune-y				:=
tune-$(CONFIG_CPU_ARM610)	:=-mtune=arm610
tune-$(CONFIG_CPU_ARM710)	:=-mtune=arm710
tune-$(CONFIG_CPU_ARM720T)	:=-mtune=arm7tdmi
tune-$(CONFIG_CPU_ARM920T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM922T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_ARM926T)	:=-mtune=arm9tdmi
tune-$(CONFIG_CPU_SA110)	:=-mtune=strongarm110
tune-$(CONFIG_CPU_SA1100)	:=-mtune=strongarm1100

CFLAGS_BOOT	:=$(apcs-y) $(arch-y) $(tune-y) -mshort-load-bytes -msoft-float -Uarm
CFLAGS		+=$(apcs-y) $(arch-y) $(tune-y) -mshort-load-bytes -msoft-float -Uarm
AFLAGS		+=$(apcs-y) $(arch-y) -mno-fpu -msoft-float

ifeq ($(CONFIG_CPU_26),y)
PROCESSOR	:= armo
  ifeq ($(CONFIG_ROM_KERNEL),y)
    DATAADDR	 = 0x02080000
    TEXTADDR	 = 0x03800000
    LDSCRIPT	 = arch/arm/vmlinux-armo-rom.lds.in
  else
    TEXTADDR	 = 0x02080000
    LDSCRIPT	 = arch/arm/vmlinux-armo.lds.in
  endif
endif

ifeq ($(CONFIG_CPU_32),y)
PROCESSOR	 = armv
TEXTADDR	 = 0xC0008000
LDSCRIPT	 = arch/arm/vmlinux-armv.lds.in
endif

ifeq ($(CONFIG_ARCH_ARCA5K),y)
MACHINE		 = arc
endif

ifeq ($(CONFIG_ARCH_RPC),y)
MACHINE		 = rpc
endif

ifeq ($(CONFIG_ARCH_EBSA110),y)
MACHINE		 = ebsa110
CFLAGS_3c589_cs	:=-DISA_SIXTEEN_BIT_PERIPHERAL
export CFLAGS_3c589_cs
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
MACHINE		 = clps7500
INCDIR		 = cl7500
endif

ifeq ($(CONFIG_ARCH_RISCSTATION),y)
MACHINE		 = riscstation
endif

ifeq ($(CONFIG_FOOTBRIDGE),y)
MACHINE		 = footbridge
INCDIR		 = ebsa285
endif

ifeq ($(CONFIG_ARCH_CO285),y)
TEXTADDR	 = 0x60008000
MACHINE		 = footbridge
INCDIR		 = ebsa285
endif

ifeq ($(CONFIG_ARCH_FTVPCI),y)
MACHINE		 = ftvpci
INCDIR		 = nexuspci
endif

ifeq ($(CONFIG_ARCH_TBOX),y)
MACHINE		 = tbox
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
MACHINE		 = shark
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
ifeq ($(CONFIG_SA1111),y)
# SA1111 DMA bug: we don't want the kernel to live in precious DMA-able memory
TEXTADDR	 = 0xc0208000
endif
MACHINE		 = sa1100
endif

ifeq ($(CONFIG_ARCH_L7200),y)
MACHINE		 = l7200
endif

ifeq ($(CONFIG_ARCH_INTEGRATOR),y)
MACHINE		 = integrator
endif

ifeq ($(CONFIG_ARCH_AT91RM9200),y)
MACHINE		 = at91rm9200
endif

ifeq ($(CONFIG_ARCH_MX1ADS),y)
MACHINE		 = mx1ads
endif

ifeq ($(CONFIG_ARCH_CAMELOT),y)
MACHINE          = epxa
endif

ifeq ($(CONFIG_ARCH_CLPS711X),y)
TEXTADDR	 = 0xc0028000
MACHINE		 = clps711x
endif

ifeq ($(CONFIG_ARCH_FORTUNET),y)
TEXTADDR	 = 0xc0208000
endif

ifeq ($(CONFIG_ARCH_ANAKIN),y)
MACHINE		 = anakin
endif

ifeq ($(CONFIG_ARCH_OMAHA),y)
MACHINE                = omaha
endif

export	MACHINE PROCESSOR TEXTADDR GZFLAGS CFLAGS_BOOT OBJCOPYFLAGS

# Only set INCDIR if its not already defined above
# Grr, ?= doesn't work as all the other assignment operators do.  Make bug?
ifeq ($(origin INCDIR), undefined)
INCDIR		:=$(MACHINE)
endif

ifeq ($(origin DATAADDR), undefined)
DATAADDR	:= .
endif

# If we have a machine-specific directory, then include it in the build.
MACHDIR		:= arch/arm/mach-$(MACHINE)
ifeq ($(MACHDIR),$(wildcard $(MACHDIR)))
SUBDIRS		+= $(MACHDIR)
CORE_FILES	:= $(MACHDIR)/$(MACHINE).o $(CORE_FILES)
endif

HEAD		:=arch/arm/kernel/head-$(PROCESSOR).o \
		  arch/arm/kernel/init_task.o
SUBDIRS		+=arch/arm/kernel arch/arm/mm arch/arm/lib arch/arm/nwfpe
CORE_FILES	:=arch/arm/kernel/kernel.o arch/arm/mm/mm.o $(CORE_FILES)
LIBS		:=arch/arm/lib/lib.a $(LIBS)

ifeq ($(CONFIG_FPE_NWFPE),y)
LIBS		:=arch/arm/nwfpe/math-emu.o $(LIBS)
endif

# Only include fastfpe if it is part of the kernel tree.
FASTFPE		:=arch/arm/fastfpe
ifeq ($(FASTFPE),$(wildcard $(FASTFPE)))
SUBDIRS		+=$(FASTFPE)
ifeq ($(CONFIG_FPE_FASTFPE),y)
LIBS		:=arch/arm/fastfpe/fast-math-emu.o $(LIBS)
endif
endif

ifeq ($(findstring y,$(CONFIG_USB_OHCI_SA1111) $(CONFIG_USB_OHCI_AT91)),y)
   SUBDIRS	+=arch/arm/common
   CORE_FILES	+=arch/arm/common/nopci.o
endif

ifeq ($(findstring y,$(CONFIG_ARCH_CLPS7500) $(CONFIG_ARCH_L7200)),y)
SUBDIRS		+=drivers/acorn/char
DRIVERS		+=drivers/acorn/char/acorn-char.o
endif

ifeq ($(CONFIG_ARCH_RISCSTATION),y)
SUBDIRS		+=drivers/acorn/char
DRIVERS		+=drivers/acorn/char/acorn-char.o
endif

MAKEBOOT	 =$(MAKE) -C arch/$(ARCH)/boot
MAKETOOLS	 =$(MAKE) -C arch/$(ARCH)/tools

# The following is a hack to get 'constants.h' up
# to date before starting compilation

$(patsubst %,_dir_%, $(SUBDIRS)): maketools
$(patsubst %,_modsubdir_%,$(MOD_DIRS)): maketools

symlinks: include/asm-arm/.arch include/asm-arm/.proc

#	Update machine arch and proc symlinks if something which affects
#	them changed.  We use .arch and .proc to indicate when they were
#	updated last, otherwise make uses the target directory mtime.

include/asm-arm/.arch: $(wildcard include/config/arch/*.h)
ifneq ("$(INCDIR)","")
	@echo '  Making asm-arm/arch -> asm-arm/arch-$(INCDIR) symlink'
	@rm -f include/asm-arm/arch
	@ln -sf arch-$(INCDIR) include/asm-arm/arch
	@touch $@
else
	@echo '  No architecture defined.  You may want to use a pre-packaged config.  make a5k_config, ebsa110_config, footbridge_config, etc.'
endif

include/asm-arm/.proc: $(wildcard include/config/cpu/32.h) $(wildcard include/config/cpu/26.h)
ifneq ("$(INCDIR)","")
	@echo '  Making asm-arm/proc -> asm-arm/proc-$(PROCESSOR) symlink'
	@rm -f include/asm-arm/proc
	@ln -sf proc-$(PROCESSOR) include/asm-arm/proc
	@touch $@
else
	@echo '  No architecture defined.  You may want to use a pre-packaged config.  make a5k_config, ebsa110_config, footbridge_config, etc.'
endif

.PHONY: maketools
maketools: include/asm-arm/.arch include/asm-arm/.proc \
        include/asm-arm/constants.h include/linux/version.h checkbin
	@$(MAKETOOLS)

vmlinux: arch/arm/vmlinux.lds

arch/arm/vmlinux.lds: arch/arm/Makefile $(LDSCRIPT) \
	$(wildcard include/config/cpu/32.h) \
	$(wildcard include/config/cpu/26.h) \
	$(wildcard include/config/arch/*.h)
	@echo '  Generating $@'
	@sed 's/TEXTADDR/$(TEXTADDR)/;s/DATAADDR/$(DATAADDR)/' $(LDSCRIPT) >$@

arch/arm/kernel arch/arm/mm arch/arm/lib: dummy
	$(MAKE) CFLAGS="$(CFLAGS) $(CFLAGS_KERNEL)" $(subst $@, _dir_$@, $@)

bzImage zImage zinstall Image bootpImage install: vmlinux
	@$(MAKEBOOT) $@

CLEAN_FILES	+= \
	arch/arm/vmlinux.lds

MRPROPER_FILES	+= \
	arch/arm/tools/constants.h* \
	include/asm-arm/arch include/asm-arm/.arch \
	include/asm-arm/proc include/asm-arm/.proc \
	include/asm-arm/constants.h* \
	include/asm-arm/mach-types.h

# We use MRPROPER_FILES and CLEAN_FILES now
archmrproper: FORCE
	@/bin/true

archclean: FORCE
	@$(MAKEBOOT) clean

archdep: scripts/mkdep symlinks
	@$(MAKETOOLS) dep
	@$(MAKEBOOT) dep

# Ensure this is ld "2.9.5" or later
NEW_LINKER	:= $(shell $(LD) --gc-sections --version >/dev/null 2>&1; echo $$?)

ifneq ($(NEW_LINKER),0)
checkbin: FORCE
	@echo '*** ${VERSION}.${PATCHLEVEL} kernels no longer build correctly with old versions of binutils.'
	@echo '*** Please upgrade your binutils to 2.9.5.'
	@false
else
checkbin: FORCE
	@true
endif

.PHONY: FORCE

# My testing targets (that short circuit a few dependencies)
zImg:;	@$(MAKEBOOT) zImage
Img:;	@$(MAKEBOOT) Image
i:;	@$(MAKEBOOT) install
zi:;	@$(MAKEBOOT) zinstall
bp:;	@$(MAKEBOOT) bootpImage

#
# Configuration targets.  Use these to select a
# configuration for your architecture
%_config:
	@( \
	CFG=$(@:_config=); \
	if [ -f arch/arm/def-configs/$$CFG ]; then \
	  [ -f .config ] && mv -f .config .config.old; \
	  cp arch/arm/def-configs/$$CFG .config; \
	  echo "*** Default configuration for $$CFG installed"; \
	  echo "*** Next, you may run 'make oldconfig'"; \
	else \
	  echo "$$CFG does not exist"; \
	fi; \
	)
