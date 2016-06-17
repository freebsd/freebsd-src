#
# Makefile for the Linux msdos filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := msdos.o

export-objs := msdosfs_syms.o

obj-y := namei.o msdosfs_syms.o
obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
