#
# Makefile for the Linux LAPB layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...


O_TARGET := lapb.o

export-objs := lapb_iface.o

obj-y	 := lapb_in.o lapb_out.o lapb_subr.o lapb_timer.o lapb_iface.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make

