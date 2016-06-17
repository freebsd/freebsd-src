# Makefile for linux/drivers/net/arcnet
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

obj-y	:=
obj-n	:=
obj-m	:=
obj-	:=
export-objs := arcnet.o com20020.o

obj-$(CONFIG_ARCNET) += arcnet.o
obj-$(CONFIG_ARCNET_1201) += rfc1201.o
obj-$(CONFIG_ARCNET_1051) += rfc1051.o
obj-$(CONFIG_ARCNET_RAW) += arc-rawmode.o
obj-$(CONFIG_ARCNET_COM90xx) += com90xx.o
obj-$(CONFIG_ARCNET_COM90xxIO) += com90io.o
obj-$(CONFIG_ARCNET_RIM_I) += arc-rimi.o
obj-$(CONFIG_ARCNET_COM20020) += com20020.o
obj-$(CONFIG_ARCNET_COM20020_ISA) += com20020-isa.o
obj-$(CONFIG_ARCNET_COM20020_PCI) += com20020-pci.o

O_TARGET := arcnetdrv.o

include $(TOPDIR)/Rules.make
