#
# Makefile for the Linux proc filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := proc.o

export-objs := root.o

obj-y    := inode.o root.o base.o generic.o array.o \
		kmsg.o proc_tty.o proc_misc.o kcore.o

ifeq ($(CONFIG_PROC_DEVICETREE),y)
obj-y += proc_devtree.o
endif

include $(TOPDIR)/Rules.make
