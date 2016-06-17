#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).

USE_STANDARD_AS_RULE := true

O_TARGET		:= footbridge.o

# Object file lists.

obj-y			:= arch.o irq.o mm.o #dma.o
obj-m			:=
obj-n			:=
obj-			:=

export-objs		:= arch.o netwinder-hw.o

pci-$(CONFIG_ARCH_CATS) += cats-pci.o
pci-$(CONFIG_ARCH_EBSA285) += ebsa285-pci.o
pci-$(CONFIG_ARCH_NETWINDER) += netwinder-pci.o
pci-$(CONFIG_ARCH_PERSONAL_SERVER) += personal-pci.o

leds-$(CONFIG_ARCH_CO285) += ebsa285-leds.o
leds-$(CONFIG_ARCH_EBSA285) += ebsa285-leds.o
leds-$(CONFIG_ARCH_NETWINDER) += netwinder-leds.o

obj-$(CONFIG_ARCH_CATS) += cats-hw.o
obj-$(CONFIG_ARCH_NETWINDER) += netwinder-hw.o

obj-$(CONFIG_PCI)	+=$(pci-y)
obj-$(CONFIG_LEDS)	+=$(leds-y)

include $(TOPDIR)/Rules.make
