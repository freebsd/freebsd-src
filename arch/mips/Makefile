#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1994, 1995, 1996 by Ralf Baechle
# DECStation modifications by Paul M. Antoine, 1996
# Copyright (C) 2002, 2003  Maciej W. Rozycki
#
# This file is included by the global makefile so that you can add your own
# architecture-specific flags and dependencies. Remember to do have actions
# for "archclean" and "archdep" for cleaning up and making dependencies for
# this architecture
#

#
# Select the object file format to substitute into the linker script.
#
ifdef CONFIG_CPU_LITTLE_ENDIAN
tool-prefix	= mipsel-linux-
ld-emul		= elf32ltsmip
else
tool-prefix	= mips-linux-
ld-emul		= elf32btsmip
endif

ifdef CONFIG_CROSSCOMPILE
CROSS_COMPILE	= $(tool-prefix)
endif

check_gcc = $(shell if $(CC) $(1) -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then echo "$(1)"; else echo "$(2)"; fi)

#
#
# GCC uses -G 0 -mabicalls -fpic as default.  We don't want PIC in the kernel
# code since it only slows down the whole thing.  At some point we might make
# use of global pointer optimizations but their use of $28 conflicts with
# the current pointer optimization.
#
# The DECStation requires an ECOFF kernel for remote booting, other MIPS
# machines may also.  Since BFD is incredibly buggy with respect to
# crossformat linking we rely on the elf2ecoff tool for format conversion.
#
GCCFLAGS	:= -I $(TOPDIR)/include/asm/gcc
GCCFLAGS	+= -G 0 -mno-abicalls -fno-pic -pipe
GCCFLAGS	+= $(call check_gcc, -finline-limit=100000,)
LINKFLAGS	+= -G 0 -static # -N
MODFLAGS	+= -mlong-calls

ifdef CONFIG_DEBUG_INFO
GCCFLAGS	+= -g
ifdef CONFIG_SB1XXX_CORELIS
GCCFLAGS	+= -mno-sched-prolog -fno-omit-frame-pointer
endif
endif

#
# Use: $(call set_gccflags,<cpu0>,<isa0>,<cpu1>,<isa1>,<isa2>)
#
# <cpu0>,<isa0> -- preferred CPU and ISA designations (may require
#                  recent tools)
# <cpu1>,<isa1> -- fallback CPU and ISA designations (have to work
#                  with up to the oldest supported tools)
# <isa2>        -- an ISA designation used as an ABI selector for
#                  gcc versions that do not support "-mabi=32"
#                  (depending on the CPU type, either "mips1" or
#                  "mips2")
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
gcc_abi=-mabi=32; gcc_cpu=$$cpu; \
if $(CC) $$gcc_abi -S -o /dev/null -xc /dev/null > /dev/null 2>&1; then \
	gcc_isa=$$isa; \
else \
	gcc_abi=; gcc_isa=-$(5); \
fi; \
gas_abi=-Wa,-32; gas_cpu=$$cpu; gas_isa=-Wa,$$isa; \
while :; do \
	for gas_opt in -Wa,-march= -Wa,-mcpu=; do \
		$(CC) $$gas_abi $$gas_opt$$cpu $$gas_isa -Wa,-Z -c \
			-o /dev/null -xassembler /dev/null > /dev/null 2>&1 && \
			break 2; \
	done; \
	gas_abi=; gas_opt=; gas_cpu=; gas_isa=; \
	break; \
done; \
echo $$gcc_abi $$gcc_opt$$gcc_cpu $$gcc_isa $$gas_abi $$gas_opt$$gas_cpu $$gas_isa)

#
# CPU-dependent compiler/assembler options for optimization.
#
ifdef CONFIG_CPU_R3000
GCCFLAGS	+= $(call set_gccflags,r3000,mips1,r3000,mips1,mips1)
endif
ifdef CONFIG_CPU_TX39XX
GCCFLAGS	+= $(call set_gccflags,r3900,mips1,r3000,mips1,mips1)
endif
ifdef CONFIG_CPU_R6000
GCCFLAGS	+= $(call set_gccflags,r6000,mips2,r6000,mips2,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_R4300
GCCFLAGS	+= $(call set_gccflags,r4300,mips3,r4300,mips3,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_VR41XX
GCCFLAGS	+= $(call set_gccflags,r4100,mips3,r4600,mips3,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_R4X00
GCCFLAGS	+= $(call set_gccflags,r4600,mips3,r4600,mips3,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_TX49XX
GCCFLAGS	+= $(call set_gccflags,r4600,mips3,r4600,mips3,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_MIPS32
GCCFLAGS	+= $(call set_gccflags,mips32,mips32,r4600,mips3,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_MIPS64
GCCFLAGS	+= $(call set_gccflags,mips64,mips64,r4600,mips3,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_R5000
GCCFLAGS	+= $(call set_gccflags,r5000,mips4,r5000,mips4,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_R5432
GCCFLAGS	+= $(call set_gccflags,r5400,mips4,r5000,mips4,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_NEVADA
GCCFLAGS	+= $(call set_gccflags,rm5200,mips4,r5000,mips4,mips2) \
		   -Wa,--trap
#GCCFLAGS	+= $(call check_gcc,-mmad,)
endif
ifdef CONFIG_CPU_RM7000
GCCFLAGS	+= $(call set_gccflags,rm7000,mips4,r5000,mips4,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_RM9000
GCCFLAGS	+= $(call set_gccflags,rm9000,mips4,r5000,mips4,mips2) \
		   -Wa,--trap
endif
ifdef CONFIG_CPU_SB1
GCCFLAGS	+= $(call set_gccflags,sb1,mips64,r5000,mips4,mips2) \
		   -Wa,--trap
ifdef CONFIG_SB1_PASS_1_WORKAROUNDS
MODFLAGS	+= -msb1-pass1-workarounds
endif
endif

AFLAGS		+= $(GCCFLAGS)
CFLAGS		+= $(GCCFLAGS)
LDFLAGS		+= -m $(ld-emul)


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
# Acer PICA 61, Mips Magnum 4000 and Olivetti M700.
#
ifdef CONFIG_MIPS_JAZZ
CORE_FILES	+= arch/mips/jazz/jazz.o
SUBDIRS		+= arch/mips/jazz arch/mips/arc
LIBS		+= arch/mips/arc/arclib.a
LOADADDR	:= 0x80080000
endif

#
# Au1000 (Alchemy Semi PB1000) eval board
#
ifdef CONFIG_MIPS_PB1000
LIBS		+= arch/mips/au1000/pb1000/pb1000.o \
		   arch/mips/au1000/common/au1000.o
SUBDIRS		+= arch/mips/au1000/pb1000 arch/mips/au1000/common
LOADADDR	:= 0x80100000
endif

#
# Au1100 (Alchemy Semi PB1100) eval board
#
ifdef CONFIG_MIPS_PB1100
LIBS          += arch/mips/au1000/pb1100/pb1100.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/pb1100 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

#
# Au1500 (Alchemy Semi PB1500) eval board
#
ifdef CONFIG_MIPS_PB1500
LIBS		+= arch/mips/au1000/pb1500/pb1500.o \
		   arch/mips/au1000/common/au1000.o
SUBDIRS		+= arch/mips/au1000/pb1500 arch/mips/au1000/common
LOADADDR	:= 0x80100000
endif

#
# Au1x00 (AMD/Alchemy) eval boards
#
ifdef CONFIG_MIPS_DB1000
LIBS          += arch/mips/au1000/db1x00/db1x00.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/db1x00 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_DB1500
LIBS          += arch/mips/au1000/db1x00/db1x00.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/db1x00 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_DB1100
LIBS          += arch/mips/au1000/db1x00/db1x00.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/db1x00 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_HYDROGEN3
LIBS          += arch/mips/au1000/hydrogen3/hydrogen3.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/hydrogen3 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_BOSPORUS
LIBS          += arch/mips/au1000/db1x00/db1x00.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/db1x00 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_MIRAGE
LIBS          += arch/mips/au1000/db1x00/db1x00.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/db1x00 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_XXS1500
LIBS          += arch/mips/au1000/xxs1500/xxs1500.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/xxs1500 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_MTX1
LIBS          += arch/mips/au1000/mtx-1/mtx-1.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/mtx-1 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif

ifdef CONFIG_MIPS_PB1550
LIBS          += arch/mips/au1000/pb1550/pb1550.o \
                 arch/mips/au1000/common/au1000.o
SUBDIRS       += arch/mips/au1000/pb1550 arch/mips/au1000/common
LOADADDR      += 0x80100000
endif


#
# Cogent CSB250
#
ifdef CONFIG_COGENT_CSB250
LIBS		+= arch/mips/au1000/csb250/csb250.o \
		   arch/mips/au1000/common/au1000.o
SUBDIRS		+= arch/mips/au1000/csb250 arch/mips/au1000/common
LOADADDR	:= 0x80100000
endif

ifdef CONFIG_PCI
CORE_FILES    += arch/mips/pci/pci-core.o
SUBDIRS       += arch/mips/pci
endif

#
# Algorithmics P4032
#
ifdef CONFIG_ALGOR_P4032
CORE_FILES	+= arch/mips/algor/algor.o
SUBDIRS		+= arch/mips/algor
LOADADDR	:= 0x80000000
endif

#
# Baget/MIPS
#
ifdef CONFIG_BAGET_MIPS
SUBDIRS		+= arch/mips/baget arch/mips/baget/prom
LIBS		+= arch/mips/baget/baget.a arch/mips/baget/prom/bagetlib.a
LOADADDR	:= 0x80001000
endif

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
# Galileo EV96100 Board
#
ifdef CONFIG_MIPS_EV96100
LIBS		+= arch/mips/galileo-boards/ev96100/ev96100.o
SUBDIRS		+= arch/mips/galileo-boards/ev96100
LOADADDR	:= 0x80100000
endif

#
# Globespan IVR eval board with QED 5231 CPU
#
ifdef CONFIG_MIPS_IVR
LIBS		+= arch/mips/ite-boards/ivr/ivr.o \
		   arch/mips/ite-boards/generic/it8172.o
SUBDIRS		+= arch/mips/ite-boards/generic arch/mips/ite-boards/ivr
LOADADDR	:= 0x80100000
endif

#
# HP LaserJet
#
ifdef CONFIG_HP_LASERJET
SUBDIRS		+= arch/mips/hp-lj
LIBS		+= arch/mips/hp-lj/hp-lj.o
LOADADDR	:= 0x80030000
endif

#
# ITE 8172 eval board with QED 5231 CPU
#
ifdef CONFIG_MIPS_ITE8172
LIBS		+= arch/mips/ite-boards/qed-4n-s01b/ite.o \
		   arch/mips/ite-boards/generic/it8172.o
SUBDIRS		+= arch/mips/ite-boards/generic arch/mips/ite-boards/qed-4n-s01b
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
CORE_FILES	+= arch/mips/gt64120/common/gt64120.o \
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
# The Ocelot-C[S] setup.o must be linked early - it does the ioremap() for the
# mips_io_port_base.
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

#
# NEC DDB Vrc-5074
#
ifdef CONFIG_DDB5074
SUBDIRS		+= arch/mips/ddb5xxx/common arch/mips/ddb5xxx/ddb5074
LIBS		+= arch/mips/ddb5xxx/common/ddb5xxx.o arch/mips/ddb5xxx/ddb5074/ddb5074.o
LOADADDR	:= 0x80080000
endif

#
# NEC DDB Vrc-5476
#
ifdef CONFIG_DDB5476
SUBDIRS		+= arch/mips/ddb5xxx/common arch/mips/ddb5xxx/ddb5476
LIBS		+= arch/mips/ddb5xxx/common/ddb5xxx.o \
		   arch/mips/ddb5xxx/ddb5476/ddb5476.o
LOADADDR	:= 0x80080000
endif

#
# NEC DDB Vrc-5477
#
ifdef CONFIG_DDB5477
SUBDIRS		+= arch/mips/ddb5xxx/common arch/mips/ddb5xxx/ddb5477
LIBS		+= arch/mips/ddb5xxx/common/ddb5xxx.o \
		   arch/mips/ddb5xxx/ddb5477/ddb5477.o
LOADADDR	:= 0x80100000
endif

ifdef CONFIG_LASAT
LIBS          += arch/mips/lasat/lasatkern.o
SUBDIRS       += arch/mips/lasat
LOADADDR      += 0x80000000
endif
#
# NEC Osprey (vr4181) board
#
ifdef CONFIG_NEC_OSPREY
SUBDIRS		+= arch/mips/vr4181/common arch/mips/vr4181/osprey
LIBS		+= arch/mips/vr4181/common/vr4181.o \
		   arch/mips/vr4181/osprey/osprey.o
LOADADDR	:= 0x80002000
endif

#
# NEC Eagle/Hawk (VR4122/VR4131) board
#
ifdef CONFIG_NEC_EAGLE
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/nec-eagle
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/nec-eagle/eagle.o
LOADADDR	:= 0x80000000
endif

#
# ZAO Networks Capcella (VR4131)
#
ifdef CONFIG_ZAO_CAPCELLA
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/zao-capcella
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/zao-capcella/capcella.o
LOADADDR	:= 0x80000000
endif

#
# Victor MP-C303/304 (VR4122)
#
ifdef CONFIG_VICTOR_MPC30X
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/victor-mpc30x
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/victor-mpc30x/mpc30x.o
LOADADDR	:= 0x80001000
endif

#
# IBM WorkPad z50 (VR4121)
#
ifdef CONFIG_IBM_WORKPAD
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/ibm-workpad
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/ibm-workpad/workpad.o
LOADADDR	+= 0x80004000
endif

#
# CASIO CASSIPEIA E-55/65 (VR4111)
#
ifdef CONFIG_CASIO_E55
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/casio-e55
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/casio-e55/e55.o
LOADADDR	+= 0x80004000
endif

#
# TANBAC TB0226 Mbase (VR4131)
#
ifdef CONFIG_TANBAC_TB0226
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/tanbac-tb0226
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/tanbac-tb0226/tb0226.o
LOADADDR	:= 0x80000000
endif

#
# TANBAC TB0229 (VR4131DIMM)
#
ifdef CONFIG_TANBAC_TB0229
SUBDIRS		+= arch/mips/vr41xx/common \
		   arch/mips/vr41xx/tanbac-tb0229
CORE_FILES	+= arch/mips/vr41xx/common/vr41xx.o \
		   arch/mips/vr41xx/tanbac-tb0229/tb0229.o
LOADADDR	:= 0x80000000
endif

#
# Philips Nino
#
ifdef CONFIG_NINO
CORE_FILES	+= arch/mips/philips/nino/nino.o
SUBDIRS		+= arch/mips/philips/nino
LOADADDR	:= 0x80000000
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
# 0x88002000 for production kernels.  Note that the value must be
# 8kb aligned or the handling of the current variable will break.
#
LOADADDR	:= 0x88002000
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
LOADADDR	:= 0x80100000
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
# SNI RM200 PCI
#
ifdef CONFIG_SNI_RM200_PCI
CORE_FILES	+= arch/mips/sni/sni.o
SUBDIRS		+= arch/mips/sni arch/mips/arc
LIBS		+= arch/mips/arc/arclib.a
LOADADDR	:= 0x80080000
endif

#
# Toshiba JMR-TX3927 board
#
ifdef CONFIG_TOSHIBA_JMR3927
CORE_FILES	+= arch/mips/jmr3927/rbhma3100/jmr3927.o \
		   arch/mips/jmr3927/common/tx3927.o
SUBDIRS		+= arch/mips/jmr3927/rbhma3100 arch/mips/jmr3927/common
LOADADDR	:= 0x80050000
endif

#
# Toshiba RBTX4927 board or
# Toshiba RBTX4937 board
#
ifdef CONFIG_TOSHIBA_RBTX4927
MIPS           = arch/mips
CEC            = tx4927
COMMON         = $(MIPS)/$(CEC)/common
BOARD          = $(MIPS)/$(CEC)/toshiba_rbtx4927
LIBS          += $(BOARD)/toshiba_rbtx4927.o $(COMMON)/tx4927.o
SUBDIRS       += $(BOARD)                    $(COMMON)
LOADADDR      += 0x80020000
endif

#
# Choosing incompatible machines durings configuration will result in
# error messages during linking.  Select a default linkscript if
# none has been choosen above.
#
vmlinux: arch/$(ARCH)/ld.script

arch/$(ARCH)/ld.script: arch/$(ARCH)/ld.script.in arch/$(ARCH)/Makefile
	sed -e 's/@@LOADADDR@@/$(LOADADDR)/' <$< >$@
LINKFLAGS	+= -T arch/$(ARCH)/ld.script

HEAD := arch/mips/kernel/head.o arch/mips/kernel/init_task.o

SUBDIRS := $(addprefix arch/mips/, tools) $(SUBDIRS) $(addprefix arch/mips/, kernel mm lib)
CORE_FILES := arch/mips/kernel/kernel.o arch/mips/mm/mm.o $(CORE_FILES)
LIBS := arch/mips/lib/lib.a $(LIBS)

ifdef CONFIG_BAGET_MIPS

BAGETBOOT = $(MAKE) -C arch/$(ARCH)/baget

balo: vmlinux
	$(BAGETBOOT) balo

endif

ifdef CONFIG_MIPS_EV64120
gboot: vmlinux
	$(MAKE) -C arch/$(ARCH)/galileo-boards/ev64120/compressed
endif

ifdef CONFIG_LASAT
rom.bin rom.sw: vmlinux
	$(MAKE) -C arch/$(ARCH)/lasat/image $@
endif

MAKEBOOT = $(MAKE) -C arch/$(ARCH)/boot

vmlinux.ecoff: vmlinux
	@$(MAKEBOOT) $@

vmlinux.srec: vmlinux
	@$(MAKEBOOT) $@

archclean:
	@$(MAKEBOOT) clean
	rm -f arch/$(ARCH)/ld.script
	$(MAKE) -C arch/$(ARCH)/tools clean
	$(MAKE) -C arch/mips/baget clean
	$(MAKE) -C arch/mips/lasat clean

archmrproper:
	@$(MAKEBOOT) mrproper
	$(RM) $(TOPDIR)/include/asm-$(ARCH)/offset.h
	$(MAKE) -C arch/$(ARCH)/tools mrproper

archdep:
	if [ ! -f $(TOPDIR)/include/asm-$(ARCH)/offset.h ]; then \
		touch $(TOPDIR)/include/asm-$(ARCH)/offset.h; \
	fi;
	@$(MAKEBOOT) dep
