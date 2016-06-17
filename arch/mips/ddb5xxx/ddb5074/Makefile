#
# Makefile for the NEC DDB Vrc-5074 specific kernel interface routines
# under Linux.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...
#

USE_STANDARD_AS_RULE := true

O_TARGET = ddb5074.o

obj-y				+= setup.o irq.o int-handler.o pci.o pci_ops.o nile4_pic.o time.o

include $(TOPDIR)/Rules.make
