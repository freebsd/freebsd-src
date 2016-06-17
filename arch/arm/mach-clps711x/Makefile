#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= clps711x.o

# Object file lists.

obj-y			:= irq.o mm.o time.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= p720t-leds.o

obj-$(CONFIG_ARCH_AUTCPU12) += autcpu12.o
obj-$(CONFIG_ARCH_CDB89712) += cdb89712.o
obj-$(CONFIG_ARCH_CLEP7312) += clep7312.o
obj-$(CONFIG_ARCH_EDB7211) += edb7211-arch.o edb7211-mm.o
obj-$(CONFIG_ARCH_GUIDEA07) += guide-a07.o
obj-$(CONFIG_ARCH_P720T) += p720t.o
obj-$(CONFIG_ARCH_FORTUNET) += fortunet.o
leds-$(CONFIG_ARCH_P720T) += p720t-leds.o
obj-$(CONFIG_LEDS)	+= $(leds-y)

include $(TOPDIR)/Rules.make
