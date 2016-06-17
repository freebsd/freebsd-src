#
# Makefile for the linux qnx4-filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := qnx4.o

obj-y   := inode.o dir.o namei.o file.o bitmap.o truncate.o fsync.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
