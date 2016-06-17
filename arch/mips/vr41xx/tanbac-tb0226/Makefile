#
# Makefile for the TANBAC TB0226 specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := tb0226.o

all: tb0226.o

obj-y	:= init.o setup.o

obj-$(CONFIG_PCI)	+= pci_fixup.o

include $(TOPDIR)/Rules.make
