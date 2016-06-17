#
# Makefile for the Linux affs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

#EXTRA_CFLAGS=-DDEBUG=1

O_TARGET := affs.o

obj-y   := super.o namei.o inode.o file.o dir.o amigaffs.o bitmap.o symlink.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
