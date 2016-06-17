#
# Makefile for the linux BeOS filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := befs.o

obj-y    := datastream.o btree.o super.o inode.o debug.o io.o linuxvfs.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
