#
# Makefile for drivers/net/tokenring
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

obj-y   :=
obj-n   :=
obj-m   :=
obj-    :=
export-objs := tms380tr.o

obj-$(CONFIG_IBMTR) 	+= ibmtr.o
obj-$(CONFIG_IBMOL) 	+= olympic.o
obj-$(CONFIG_IBMLS) 	+= lanstreamer.o
obj-$(CONFIG_TMS380TR) 	+= tms380tr.o
obj-$(CONFIG_ABYSS) 	+= abyss.o
obj-$(CONFIG_MADGEMC) 	+= madgemc.o
obj-$(CONFIG_TMSPCI) 	+= tmspci.o
obj-$(CONFIG_TMSISA) 	+= tmsisa.o
obj-$(CONFIG_SMCTR) 	+= smctr.o
obj-$(CONFIG_3C359)	+= 3c359.o

O_TARGET := tr.o

include $(TOPDIR)/Rules.make
