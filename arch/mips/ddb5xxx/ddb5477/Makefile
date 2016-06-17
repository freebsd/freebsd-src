#
# Makefile for NEC DDB-Vrc5477 board
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= ddb5477.o

obj-y	 += int-handler.o irq.o irq_5477.o setup.o pci.o pci_ops.o lcd44780.o

obj-$(CONFIG_RUNTIME_DEBUG) 	+= debug.o
obj-$(CONFIG_KGDB)		+= kgdb_io.o

include $(TOPDIR)/Rules.make
