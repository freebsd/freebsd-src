#
# Makefile for the NEC Eagle/Hawk specific parts of the kernel
#
# Author: Yoichi Yuasa
#         yyuasa@mvista.com or source@mvista.com
#
# Copyright 2001,2002 MontaVista Software Inc.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := eagle.o

all: eagle.o

obj-y	:= init.o irq.o setup.o

obj-$(CONFIG_PCI)	+= pci_fixup.o vrc4173.o

include $(TOPDIR)/Rules.make
