#
# Makefile for Momentum Computer's Ocelot-G board.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= ocelot_g.o

obj-y	 	+= gt-irq.o pci-irq.o int-handler.o irq.o prom.o reset.o setup.o
obj-$(CONFIG_KGDB)	+= dbg_io.o
obj-$(CONFIG_PCI)	+= pci.o

include $(TOPDIR)/Rules.make
