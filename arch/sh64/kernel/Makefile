#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2000, 2001  Paolo Alberelli
#
# Makefile for the Linux SH-5 kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

.S.o:
	$(CC) $(CFLAGS) $(AFLAGS) -c $< -o $*.o

O_TARGET := kernel.o

obj-y   := process.o signal.o entry.o traps.o irq.o irq_intc.o \
	    ptrace.o setup.o time.o sys_sh64.o semaphore.o sh_ksyms.o

obj-$(CONFIG_HEARTBEAT)		+= led.o
obj-$(CONFIG_PCI)		+= pci-dma.o pcibios.o

ifeq ($(CONFIG_PCI),y)
obj-$(CONFIG_CPU_SH5)		+= pci_sh5.o
endif

obj-$(CONFIG_SH_DMA)		+= dma.o

export-objs := sh_ksyms.o dma.o

ifndef CONFIG_NOFPU_SUPPORT
obj-y	+= fpu.o
endif

all: $(O_TARGET) head.o init_task.o

include $(TOPDIR)/Rules.make
