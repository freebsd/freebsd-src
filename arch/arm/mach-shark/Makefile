#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= shark.o

# Object file lists.

obj-y			:= arch.o dma.o mm.o pci.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= 

obj-$(CONFIG_LEDS)	+= leds.o

include $(TOPDIR)/Rules.make
