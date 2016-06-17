#
# Makefile for the linux udf-filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .o file).
#
# Note 2! The CFLAGS definitions are now in the main makefile..

O_TARGET := udf.o

obj-y    := balloc.o dir.o file.o ialloc.o inode.o lowlevel.o namei.o \
		partition.o super.o truncate.o symlink.o fsync.o \
		crc.o directory.o misc.o udftime.o unicode.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
