#
# Makefile for the Linux VLAN layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := 8021q.o

obj-y := vlan.o vlanproc.o vlan_dev.o
obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
