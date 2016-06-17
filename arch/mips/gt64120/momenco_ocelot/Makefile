#
# Makefile for Momentum's Ocelot board.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= momenco_ocelot.o

obj-y	 		+= int-handler.o irq.o  prom.o reset.o setup.o

obj-$(CONFIG_KGDB)	+= dbg_io.o
obj-$(CONFIG_PCI)	+= fixup-ocelot.o

include $(TOPDIR)/Rules.make
