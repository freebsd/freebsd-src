#
#  Copyright 2000 MontaVista Software Inc.
#  Author: MontaVista Software, Inc.
#     	ppopov@mvista.com or source@mvista.com
#
# Makefile for the Alchemy Au1000 CPU, generic files.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

all: au1000.o

O_TARGET := au1000.o

export-objs		= prom.o clocks.o power.o usbdev.o

obj-y := prom.o int-handler.o dma.o irq.o puts.o time.o reset.o \
	clocks.o power.o setup.o sleeper.o

obj-$(CONFIG_AU1X00_USB_DEVICE) += usbdev.o
obj-$(CONFIG_KGDB) += dbg_io.o
obj-$(CONFIG_RTC) += rtc.o
obj-$(CONFIG_PCI) += pci_ops.o pci_fixup.o

include $(TOPDIR)/Rules.make
