#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

USE_STANDARD_AS_RULE	:= true

ifdef CONFIG_PPC64BRIDGE
EXTRA_AFLAGS		:= -Wa,-mppc64bridge
endif
ifdef CONFIG_4xx
EXTRA_AFLAGS		:= -Wa,-m405
endif

# Code in these files is used before the kernel is moved to its final position
CFLAGS_prom_init.o	+= -mrelocatable-lib
CFLAGS_btext.o		+= -mrelocatable-lib

# Start off with 'head.o', change as needed.
HEAD-y				:= head.o
HEAD-$(CONFIG_6xx)		+= idle_6xx.o
HEAD-$(CONFIG_40x)		:= head_4xx.o
HEAD-$(CONFIG_44x)		:= head_44x.o
HEAD-$(CONFIG_8xx)		:= head_8xx.o

all: $(HEAD-y) kernel.o

O_TARGET := kernel.o

export-objs			:= ppc_ksyms.o time.o ocp.o ppc4xx_setup.o \
					ppc4xx_dma.o ppc4xx_sgdma.o \
					ppc4xx_stbdma.o

obj-y				:= entry.o traps.o irq.o idle.o time.o misc.o \
					process.o signal.o ptrace.o align.o \
					semaphore.o syscalls.o setup.o \
					cputable.o ppc_htab.o
ifneq ($(CONFIG_6xx),y)
obj-y				+= idle_gen.o
endif
obj-$(CONFIG_6xx)		+= l2cr.o cpu_setup_6xx.o
obj-$(CONFIG_MODULES)		+= ppc_ksyms.o
obj-$(CONFIG_PCI)		+= pci.o
obj-$(CONFIG_PCI)		+= pci-dma.o
obj-$(CONFIG_KGDB)		+= ppc-stub.o
obj-$(CONFIG_PPCBUG_NVRAM)	+= prep_nvram.o
obj-$(CONFIG_SMP)		+= smp.o
obj-$(CONFIG_TAU)		+= temp.o
ifeq ($(CONFIG_SERIAL)$(CONFIG_GEN550),yy)
obj-$(CONFIG_KGDB)		+= gen550_kgdb.o gen550_dbg.o
obj-$(CONFIG_SERIAL_TEXT_DEBUG)	+= gen550_dbg.o
endif
obj-$(CONFIG_PPC_OCP)		+= ocp.o
obj-$(CONFIG_40x)		+= ppc4xx_setup.o ppc4xx_pic.o
obj-$(CONFIG_4xx)		+= todc_time.o
obj-$(CONFIG_STBXXX_DMA)	+= ppc4xx_stbdma.o
ifeq ($(CONFIG_PCI),y)
obj-$(CONFIG_40x)		+= ppc405_pci.o
obj-$(CONFIG_4xx)		+= indirect_pci.o pci_auto.o
endif
obj-$(CONFIG_44x)		+= ppc4xx_pic.o ibm44x_common.o
obj-$(CONFIG_440GP)		+= ibm440gp_common.o
obj-$(CONFIG_440GX)		+= ibm440gx_common.o
obj-$(CONFIG_PPC4xx_DMA)	+= ppc4xx_dma.o
obj-$(CONFIG_PPC4xx_EDMA)	+= ppc4xx_sgdma.o
obj-$(CONFIG_8xx)		+= m8xx_setup.o ppc8xx_pic.o
obj-$(CONFIG_8xx_WDT)		+= m8xx_wdt.o
ifeq ($(CONFIG_8xx),y)
obj-$(CONFIG_PCI)		+= qspan_pci.o
ifndef CONFIG_MATH_EMULATION
obj-y				+= softemu8xx.o
endif
endif
obj-$(CONFIG_MBX)		+= i8259.o
obj-$(CONFIG_ALL_PPC)		+= prom_init.o prom.o open_pic.o \
					indirect_pci.o i8259.o
obj-$(CONFIG_GEMINI)		+= open_pic.o
obj-$(CONFIG_LOPEC)		+= mpc10x_common.o indirect_pci.o pci_auto.o \
					open_pic.o i8259.o todc_time.o
obj-$(CONFIG_PAL4)		+= indirect_pci.o pci_auto.o todc_time.o
obj-$(CONFIG_PPLUS)		+= pplus_common.o open_pic.o i8259.o \
				   indirect_pci.o todc_time.o pci_auto.o
obj-$(CONFIG_PRPMC750)		+= open_pic.o indirect_pci.o pci_auto.o \
					pplus_common.o
obj-$(CONFIG_SANDPOINT)		+= i8259.o open_pic.o mpc10x_common.o \
					pci_auto.o indirect_pci.o todc_time.o
obj-$(CONFIG_SPRUCE)		+= indirect_pci.o pci_auto.o todc_time.o
obj-$(CONFIG_8260)		+= m8260_setup.o cpm2_pic.o
obj-$(CONFIG_BOOTX_TEXT)	+= btext.o

include $(TOPDIR)/Rules.make

entry.o: entry.S ppc_defs.h
misc.o: misc.S ppc_defs.h
l2cr.o: l2cr.S ppc_defs.h
head.o: head.S ppc_defs.h
head_4xx.o: head_4xx.S ppc_defs.h
head_44x.o: head_44x.S ppc_defs.h
head_8xx.o: head_8xx.S ppc_defs.h
idle_6xx.o: idle_6xx.S ppc_defs.h

ppc_defs.h: mk_defs.c ppc_defs.head \
		$(TOPDIR)/include/asm/mmu.h \
		$(TOPDIR)/include/asm/processor.h \
		$(TOPDIR)/include/asm/pgtable.h \
		$(TOPDIR)/include/asm/ptrace.h
	$(CC) $(CFLAGS) -S mk_defs.c
	cp ppc_defs.head ppc_defs.h
# for bk, this way we can write to the file even if it's not checked out
	chmod u+w ppc_defs.h
	grep '^#define' mk_defs.s >> ppc_defs.h
	rm mk_defs.s

find_name : find_name.c
	$(HOSTCC) $(HOSTCFLAGS) -o find_name find_name.c

checks: checks.c
	$(HOSTCC) -I$(HPATH) $(HOSTCFLAGS) -D__KERNEL__ -fno-builtin -o checks checks.c
	./checks
