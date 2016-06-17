#
# Makefile for TOSHIBA JMR-TX3927 board
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= jmr3927.o

obj-y	 += init.o int-handler.o irq.o setup.o rtc.o pci_fixup.o pci_ops.o

obj-$(CONFIG_RUNTIME_DEBUG) 	+= debug.o
obj-$(CONFIG_KGDB)		+= kgdb_io.o

include $(TOPDIR)/Rules.make
