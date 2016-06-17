#
#  Copyright 2000 MontaVista Software Inc.
#  Author: MontaVista Software, Inc.
#     	ppopov@mvista.com or source@mvista.com
#
# Makefile for the Globespan IVR board,
# board-specific files.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

all: ivr.o

O_TARGET := ivr.o

obj-y := init.o

obj-$(CONFIG_PCI) += pci_fixup.o

include $(TOPDIR)/Rules.make
