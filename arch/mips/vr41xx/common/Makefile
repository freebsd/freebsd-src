#
# Makefile for common code of the NEC VR4100 series.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

all: vr41xx.o

O_TARGET := vr41xx.o

obj-y := bcu.o cmu.o giu.o icu.o int-handler.o ksyms.o reset.o rtc.o

export-objs := ksyms.o vrc4171.o vrc4173.o

obj-$(CONFIG_PCI)		+= pciu.o
obj-$(CONFIG_SERIAL)		+= serial.o
obj-$(CONFIG_VRC4171)		+= vrc4171.o
obj-$(CONFIG_VRC4173)		+= vrc4173.o
obj-$(subst m,y,$(CONFIG_IDE))	+= ide.o

include $(TOPDIR)/Rules.make
