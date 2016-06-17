#
# Makefile for drivers/telephony
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

obj-y   :=
obj-n   :=
obj-m   :=
obj-    :=
export-objs := phonedev.o ixj.o

obj-$(CONFIG_PHONE) += phonedev.o
obj-$(CONFIG_PHONE_IXJ) += ixj.o
obj-$(CONFIG_PHONE_IXJ_PCMCIA) += ixj_pcmcia.o

O_TARGET := telephony.o

include $(TOPDIR)/Rules.make

