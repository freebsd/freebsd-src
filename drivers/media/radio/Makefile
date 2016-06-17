#
# Makefile for the kernel character device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

# Object file lists.

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

O_TARGET := radio.o

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs     := miropcm20-rds-core.o

list-multi	:= miropcm20.o

miropcm20-objs	:= miropcm20-rds-core.o miropcm20-radio.o

obj-$(CONFIG_RADIO_AZTECH) += radio-aztech.o
obj-$(CONFIG_RADIO_RTRACK2) += radio-rtrack2.o
obj-$(CONFIG_RADIO_SF16FMI) += radio-sf16fmi.o
obj-$(CONFIG_RADIO_SF16FMR2) += radio-sf16fmr2.o
obj-$(CONFIG_RADIO_CADET) += radio-cadet.o
obj-$(CONFIG_RADIO_TYPHOON) += radio-typhoon.o
obj-$(CONFIG_RADIO_TERRATEC) += radio-terratec.o
obj-$(CONFIG_RADIO_MAXIRADIO) += radio-maxiradio.o
obj-$(CONFIG_RADIO_RTRACK) += radio-aimslab.o
obj-$(CONFIG_RADIO_ZOLTRIX) += radio-zoltrix.o
obj-$(CONFIG_RADIO_MIROPCM20) += miropcm20.o
obj-$(CONFIG_RADIO_MIROPCM20_RDS) += miropcm20-rds.o
obj-$(CONFIG_RADIO_GEMTEK) += radio-gemtek.o
obj-$(CONFIG_RADIO_GEMTEK_PCI) += radio-gemtek-pci.o
obj-$(CONFIG_RADIO_TRUST) += radio-trust.o
obj-$(CONFIG_RADIO_MAESTRO) += radio-maestro.o

include $(TOPDIR)/Rules.make

miropcm20.o: $(miropcm20-objs)
	$(LD) -r -o $@ $(miropcm20-objs)
