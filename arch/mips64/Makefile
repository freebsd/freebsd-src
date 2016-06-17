#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2002, 2003  Maciej W. Rozycki
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#

comma := ,

#
# Select the object file format to substitute into the linker script.
#
ifdef CONFIG_CPU_LITTLE_ENDIAN
tool-prefix	= mips64el-linux-
else
tool-prefix	= mips64-linux-
endif

ifdef CONFIG_CROSSCOMPILE
CROSS_COMPILE	= $(tool-prefix)
endif

#
# The ELF GCC uses -G 0 -mabicalls -fpic as default.  We don't need PIC
# code in the kernel since it only slows down the whole thing.  For the
# old GCC these options are just the defaults.  At some point we might
# make use of global pointer optimizations.
#
# The DECStation requires an ECOFF kernel for remote booting, other MIPS
# machines may also.  Since BFD is incredibly buggy with respect to
# crossformat linking we rely on the elf2ecoff tool for format conversion.
#
GCCFLAGS	:= -I $(TOPDIR)/include/asm/gcc
GCCFLAGS	+= -mabi=64 -G 0 -mno-abicalls -fno-pic -Wa,--trap -pipe
GCCFLAGS	+= $(call check_gcc, -finline-limit=100000,)
LINKFLAGS	+= -G 0 -static # -N
MODFLAGS	+= -mlong-calls

ifdef CONFIG_DEBUG_INFO
GCCFLAGS	+= -g
ifdef CONFIG_SB1XXX_CORELIS
GCCFLAGS	+= -mno-sched-prolog -fno-omit-frame-pointer
endif
endif

check_gcc = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi)
check_gas = $(shell if $(CC) $(1) -Wa,-Z -c -o /dev/null -xassembler /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi)

#
# Use: $(call set_gccflags,<cpu0>,<isa0>,<cpu1>,<isa1>)
#
# <cpu0>,<isa0> -- preferred CPU and ISA designations (may require
#                  recent tools)
# <cpu1>,<isa1> -- fallback CPU and ISA designations (have to work
#                  with up to the oldest supported tools)
#
set_gccflags = $(shell \
while :; do \
	cpu=$(1); isa=-$(2); \
	for gcc_opt in -march= -mcpu=; do \
		$(CC) $$gcc_opt$$cpu $$isa -S -o /dev/null \
			-xc /dev/null > /dev/null 2>&1 && \
			break 2; \
	done; \
	cpu=$(3); isa=-$(4); \
	for gcc_opt in -march= -mcpu=; do \
		$(CC) $$gcc_opt$$cpu $$isa -S -o /dev/null \
			-xc /dev/null > /dev/null 2>&1 && \
			break 2; \
	done; \
	break; \
done; \
gcc_cpu=$$cpu; gcc_isa=$$isa; \
gas_cpu=$$cpu; gas_isa=-Wa,$$isa; \
while :; do \
	for gas_opt in -Wa,-march= -Wa,-mcpu=; do \
		$(CC) $$gas_opt$$cpu $$gas_isa -Wa,-Z -c -o /dev/null \
			-xassembler /dev/null > /dev/null 2>&1 && \
			break 2; \
	done; \
	gas_opt=; gas_cpu=; gas_isa=; \
	break; \
done; \
echo $$gcc_opt$$gcc_cpu $$gcc_isa $$gas_opt$$gas_cpu $$gas_isa)

#
# CPU-dependent compiler/assembler options for optimization.
#
ifdef CONFIG_CPU_R4300
GCCFLAGS	+= $(call set_gccflags,r4300,mips3,r4300,mips3)
endif
ifdef CONFIG_CPU_R4X00
GCCFLAGS	+= $(call set_gccflags,r4600,mips3,r4600,mips3)
endif
ifdef CONFIG_CPU_R5000
GCCFLAGS	+= $(call set_gccflags,r5000,mips4,r8000,mips4)
endif
ifdef CONFIG_CPU_NEVADA
GCCFLAGS	+= $(call set_gccflags,rm5200,mips4,r8000,mips4)
#GCCFLAGS	+= $(call check_gcc,-mmad,)
endif
ifdef CONFIG_CPU_RM7000
GCCFLAGS	+= $(call set_gccflags,rm7000,mips4,r5000,mips4)
endif
ifdef CONFIG_CPU_RM9000
GCCFLAGS	+= $(call set_gccflags,rm9000,mips4,r5000,mips4)
endif
ifdef CONFIG_CPU_R8000
GCCFLAGS	+= $(call set_gccflags,r8000,mips4,r8000,mips4)
endif
ifdef CONFIG_CPU_R10000
GCCFLAGS	+= $(call set_gccflags,r10000,mips4,r8000,mips4)
endif
ifdef CONFIG_CPU_SB1
GCCFLAGS	+= $(call set_gccflags,sb1,mips64,r5000,mips4)
ifdef CONFIG_SB1_PASS_1_WORKAROUNDS
MODFLAGS	+= -msb1-pass1-workarounds
endif
endif
ifdef CONFIG_CPU_MIPS64
GCCFLAGS	+= $(call set_gccflags,mips64,mips64,r8000,mips4)
endif

#
# We unconditionally build the math emulator
#
CORE_FILES	+= arch/mips/math-emu/fpu_emulator.o
SUBDIRS		+= arch/mips/math-emu

#
# ramdisk/initrd support
# You need a compressed ramdisk image, named ramdisk.gz in
# arch/mips/ramdisk
#
ifdef CONFIG_EMBEDDED_RAMDISK
CORE_FILES	+= arch/mips/ramdisk/ramdisk.o
SUBDIRS		+= arch/mips/ramdisk
endif


#
# Board-dependent options and extra files
#

#
# Cobalt Server
#
ifdef CONFIG_MIPS_COBALT
SUBDIRS		+= arch/mips/cobalt
CORE_FILES	+= arch/mips/cobalt/cobalt.o
LOADADDR	:= 0x80080000
endif

#
# DECstation family
#
ifdef CONFIG_DECSTATION
CORE_FILES	+= arch/mips/dec/dec.o
SUBDIRS		+= arch/mips/dec arch/mips/dec/prom
LIBS		+= arch/mips/dec/prom/rexlib.a
LOADADDR	:= 0x80040000
endif

#
# Galileo EV64120 Board
#
ifdef CONFIG_MIPS_EV64120
LIBS		+= arch/mips/gt64120/common/gt64120.o \
		   arch/mips/gt64120/ev64120/ev64120.o
SUBDIRS		+= arch/mips/gt64120/common arch/mips/gt64120/ev64120
LOADADDR	:= 0x80100000
endif

#
# MIPS Atlas board
#
ifdef CONFIG_MIPS_ATLAS
LIBS		+= arch/mips/mips-boards/atlas/atlas.o \
		   arch/mips/mips-boards/generic/mipsboards.o
SUBDIRS		+= arch/mips/mips-boards/generic arch/mips/mips-boards/atlas
LOADADDR	:= 0x80100000
endif

#
# MIPS Malta board
#
ifdef CONFIG_MIPS_MALTA
LIBS		+= arch/mips/mips-boards/malta/malta.o \
		   arch/mips/mips-boards/generic/mipsboards.o
SUBDIRS		+= arch/mips/mips-boards/malta arch/mips/mips-boards/generic
LOADADDR	:= 0x80100000
endif

#
# MIPS SEAD board
#
ifdef CONFIG_MIPS_SEAD
LIBS		+= arch/mips/mips-boards/sead/sead.o \
		   arch/mips/mips-boards/generic/mipsboards.o
SUBDIRS		+= arch/mips/mips-boards/generic arch/mips/mips-boards/sead
LOADADDR	:= 0x80100000
endif

#
# Momentum Ocelot board
#
ifdef CONFIG_MOMENCO_OCELOT
# The Ocelot setup.o must be linked early - it does the ioremap() for the
# mips_io_port_base.
CORE_FILES      += arch/mips/gt64120/common/gt64120.o \
		   arch/mips/gt64120/momenco_ocelot/momenco_ocelot.o
SUBDIRS		+= arch/mips/gt64120/common arch/mips/gt64120/momenco_ocelot
LOADADDR	:= 0x80100000
endif

#
# Momentum Ocelot-G board
#
ifdef CONFIG_MOMENCO_OCELOT_G
# The Ocelot-G setup.o must be linked early - it does the ioremap() for the
# mips_io_port_base.
CORE_FILES	+= arch/mips/momentum/ocelot_g/ocelot_g.o
SUBDIRS		+= arch/mips/momentum/ocelot_g
LOADADDR	:= 0x80100000
endif
                                                                                
#
# Momentum Ocelot-C and -CS boards
#
ifdef CONFIG_MOMENCO_OCELOT_C
CORE_FILES	+= arch/mips/momentum/ocelot_c/ocelot_c.o
SUBDIRS		+= arch/mips/momentum/ocelot_c
LOADADDR	:= 0x80100000
endif

ifdef CONFIG_MOMENCO_JAGUAR_ATX
LIBS		+= arch/mips/momentum/jaguar_atx/jaguar_atx.o
SUBDIRS		+= arch/mips/momentum/jaguar_atx
ifdef CONFIG_JAGUAR_DMALOW
LOADADDR	:= 0x88000000
else
LOADADDR	:= 0x80100000
endif
endif

ifdef CONFIG_PCI
CORE_FILES    += arch/mips/pci/pci-core.o
SUBDIRS       += arch/mips/pci
endif

#
# SGI IP22 (Indy/Indigo2)
#
ifdef CONFIG_SGI_IP22
CORE_FILES	+= arch/mips/sgi-ip22/ip22-kern.o
LIBS		+= arch/mips/arc/arclib.a
SUBDIRS		+= arch/mips/sgi-ip22 arch/mips/arc
#
# Set LOADADDR to >= 0x88069000 if you want to leave space for symmon,
# 0x88004000 for production kernels.  Note that the value must be
# 16kb aligned or the handling of the current variable will break.
#
LOADADDR	:= 0x88004000
endif

#
# SGI-IP27 (Origin200/2000)
#
ifdef CONFIG_SGI_IP27
CORE_FILES	+= arch/mips/sgi-ip27/ip27.o
LIBS		+= arch/mips/arc/arclib.a
SUBDIRS		+= arch/mips/sgi-ip27 arch/mips/arc
#
# Set LOADADDR to >= 0xc000000000300000 if you want to leave space for
# symmon, 0xc00000000001c000 for production kernels.  Note that the value
# must be 16kb aligned or the handling of the current variable will break.
#
#LOADADDR	:= 0xa80000000001c000
ifdef CONFIG_MAPPED_KERNEL
LOADADDR	:= 0xc001c000
else
LOADADDR	:= 0x8001c000
endif
endif

#
# Sibyte SB1250 SOC and Broadcom (SiByte) BCM112x SOCs
#
ifneq ($(CONFIG_SIBYTE_SB1250)$(CONFIG_SIBYTE_BCM112X),)
# This is a LIB so that it links at the end, and initcalls are later
# the sequence; but it is built as an object so that modules don't get
# removed (as happens, even if they have __initcall/module_init)
LIBS		+= arch/mips/sibyte/sb1250/sb1250.o
SUBDIRS		+= arch/mips/sibyte/sb1250
ifdef CONFIG_MIPS_UNCACHED
LOADADDR	:= 0xa0100000
else
LOADADDR	:= 0x80100000
endif
endif

#
# Sibyte boards:
#
# BCM91250A (SWARM),
# BCM91250E (Sentosa),
# BCM91120C (CRhine),
# BCM91120x (Carmel),
# BCM91125C (CRhone),
# BCM91125E (Rhone).
#
ifdef CONFIG_SIBYTE_BOARD
LIBS		+= arch/mips/sibyte/swarm/sbswarm.a
SUBDIRS		+= arch/mips/sibyte/swarm
endif

#
# Sibyte CFE firmware
#
ifdef CONFIG_SIBYTE_CFE
LIBS		+= arch/mips/sibyte/cfe/cfe.a
SUBDIRS		+= arch/mips/sibyte/cfe
endif

#
# Some machines like the Indy need 32-bit ELF binaries for booting purposes.
# Other need ECOFF, so we build a 32-bit ELF binary for them which we then
# convert to ECOFF using elf2ecoff.
#
ifdef CONFIG_BOOT_ELF32
GCCFLAGS += -Wa,-32 $(call check_gas,-Wa$(comma)-mgp64,)
LINKFLAGS += -T arch/mips64/ld.script.elf32
endif
#
# The 64-bit ELF tools are pretty broken so at this time we generate 64-bit
# ELF files from 32-bit files by conversion.
#
ifdef CONFIG_BOOT_ELF64
GCCFLAGS += -Wa,-32 $(call check_gas,-Wa$(comma)-mgp64,)
LINKFLAGS += -T arch/mips64/ld.script.elf32
#AS += -64
#LD += -m elf64bmip
#LINKFLAGS += -T arch/mips64/ld.script.elf64
endif

ifdef CONFIG_CPU_LITTLE_ENDIAN
32bit-bfd = elf32-tradlittlemips
64bit-bfd = elf64-tradlittlemips
else
32bit-bfd = elf32-tradbigmips
64bit-bfd = elf64-tradbigmips
endif


AFLAGS		+= $(GCCFLAGS)
CFLAGS		+= $(GCCFLAGS)

LD		+= --oformat $(32bit-bfd)


LINKFLAGS += -Ttext $(LOADADDR)

HEAD := arch/mips64/kernel/head.o arch/mips64/kernel/init_task.o

SUBDIRS := $(addprefix arch/mips/, tools) $(SUBDIRS) $(addprefix arch/mips64/, kernel mm lib)
CORE_FILES := arch/mips64/kernel/kernel.o arch/mips64/mm/mm.o $(CORE_FILES)
LIBS := arch/mips64/lib/lib.a $(LIBS)

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

vmlinux: arch/mips64/ld.script.elf32
arch/mips64/ld.script.elf32: arch/mips64/ld.script.elf32.S
	$(CPP) -C -P -I$(HPATH) -imacros $(HPATH)/asm-mips64/sn/mapped_kernel.h -Umips arch/mips64/ld.script.elf32.S > arch/mips64/ld.script.elf32

ifdef CONFIG_MAPPED_KERNEL
vmlinux.64: vmlinux
	$(OBJCOPY) -O $(64bit-bfd) --remove-section=.reginfo --change-addresses=0xc000000080000000 $< $@
else
vmlinux.64: vmlinux
	$(OBJCOPY) -O $(64bit-bfd) --remove-section=.reginfo --change-addresses=0xa800000080000000 $< $@
endif

vmlinux.ecoff: vmlinux
	@$(MAKEBOOT) $@

archclean:
	@$(MAKEBOOT) clean
	$(MAKE) -C arch/mips/tools clean
	rm -f vmlinux.64 arch/$(ARCH)/ld.script.elf32

archmrproper:
	@$(MAKEBOOT) mrproper
	$(MAKE) -C arch/mips/tools mrproper
	$(RM) $(TOPDIR)/include/asm-$(ARCH)/offset.h

archdep:
	if [ ! -f $(TOPDIR)/include/asm-$(ARCH)/offset.h ]; then \
		touch $(TOPDIR)/include/asm-$(ARCH)/offset.h; \
	fi;
	@$(MAKEBOOT) dep
