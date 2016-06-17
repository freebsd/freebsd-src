#
# Makefile for Toshbia's RBTX4927 board
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET:= toshiba_rbtx4927.o

obj-y	+= toshiba_rbtx4927_prom.o 
obj-y	+= toshiba_rbtx4927_setup.o 
obj-y	+= toshiba_rbtx4927_irq.o 
obj-y	+= toshiba_rbtx4927_led.o
obj-y	+= toshiba_rbtx4927_nmi.o

obj-$(CONFIG_PCI)	+= toshiba_rbtx4927_pci_fixup.o 
obj-$(CONFIG_PCI)	+= toshiba_rbtx4927_pci_ops.o 
obj-$(CONFIG_DS1742)	+= toshiba_rbtx4927_rtc.o

include $(TOPDIR)/Rules.make
