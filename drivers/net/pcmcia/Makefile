#
# drivers/net/pcmcia/Makefile
#
# Makefile for the Linux PCMCIA network device drivers.
#

O_TARGET := pcmcia_net.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

# Things that need to export symbols
export-objs	:= ray_cs.o

# 16-bit client drivers
obj-$(CONFIG_PCMCIA_3C589)	+= 3c589_cs.o
obj-$(CONFIG_PCMCIA_3C574)	+= 3c574_cs.o
obj-$(CONFIG_PCMCIA_FMVJ18X)	+= fmvj18x_cs.o
obj-$(CONFIG_PCMCIA_NMCLAN)	+= nmclan_cs.o
obj-$(CONFIG_PCMCIA_PCNET)	+= pcnet_cs.o
obj-$(CONFIG_PCMCIA_SMC91C92)	+= smc91c92_cs.o
obj-$(CONFIG_PCMCIA_XIRC2PS)	+= xirc2ps_cs.o
obj-$(CONFIG_ARCNET_COM20020_CS)+= com20020_cs.o
obj-$(CONFIG_PCMCIA_AXNET)	+= axnet_cs.o

# 16-bit wireless client drivers
obj-$(CONFIG_PCMCIA_RAYCS)	+= ray_cs.o
obj-$(CONFIG_PCMCIA_NETWAVE)	+= netwave_cs.o
obj-$(CONFIG_PCMCIA_WAVELAN)	+= wavelan_cs.o
obj-$(CONFIG_AIRONET4500_CS)	+= aironet4500_cs.o

# Cardbus client drivers
obj-$(CONFIG_PCMCIA_XIRTULIP)	+= xircom_tulip_cb.o
obj-$(CONFIG_PCMCIA_XIRCOM)	+= xircom_cb.o

obj-$(CONFIG_PCMCIA_IBMTR)	+= ibmtr_cs.o

include $(TOPDIR)/Rules.make

