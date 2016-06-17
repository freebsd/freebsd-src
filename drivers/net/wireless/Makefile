#
# drivers/net/wireless/Makefile
#
# Makefile for the Linux Wireless network device drivers.
#

O_TARGET := wireless_net.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Things that need to export symbols
export-objs	:= airo.o orinoco.o hermes.o atmel.o

obj-$(CONFIG_HERMES)		+= orinoco.o hermes.o
obj-$(CONFIG_PCMCIA_HERMES)	+= orinoco_cs.o
obj-$(CONFIG_APPLE_AIRPORT)	+= airport.o
obj-$(CONFIG_PLX_HERMES)	+= orinoco_plx.o
obj-$(CONFIG_TMD_HERMES)	+= orinoco_tmd.o
obj-$(CONFIG_PCI_HERMES)	+= orinoco_pci.o

obj-$(CONFIG_AIRO)		+= airo.o
obj-$(CONFIG_AIRO_CS)		+= airo_cs.o airo.o
obj-$(CONFIG_PCMCIA_ATMEL)	+= atmel_cs.o atmel.o

include $(TOPDIR)/Rules.make
