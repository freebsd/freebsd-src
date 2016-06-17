#
# Makefile for the nubus specific drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#

L_TARGET := nubus.a

export-objs := nubus_syms.o

obj-y   := nubus.o

obj-$(CONFIG_MODULES) += nubus_syms.o 
obj-$(CONFIG_PROC_FS) += proc.o

include $(TOPDIR)/Rules.make
