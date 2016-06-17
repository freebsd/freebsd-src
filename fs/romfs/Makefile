#
# Makefile for the linux romfs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := romfs.o

obj-y  := inode.o
obj-m  := $(O_TARGET)

include $(TOPDIR)/Rules.make
