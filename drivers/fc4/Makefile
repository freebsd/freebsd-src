# File: drivers/fc4/Makefile
#
# Makefile for the Linux Fibre Channel device drivers.
#

L_TARGET := fc4.a

export-objs := fc_syms.o

list-multi := fc4.o
fc4-objs := fc.o fc_syms.o

obj-$(CONFIG_FC4) += fc4.o
obj-$(CONFIG_FC4_SOC) += soc.o
obj-$(CONFIG_FC4_SOCAL) += socal.o

include $(TOPDIR)/Rules.make

fc4.o: $(fc4-objs)
	$(LD) -r -o $@ $(fc4-objs)
