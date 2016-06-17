#
# Makefile for the Linux ufs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := ufs.o

obj-y   := balloc.o cylinder.o dir.o file.o ialloc.o inode.o \
		namei.o super.o symlink.o truncate.o util.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
