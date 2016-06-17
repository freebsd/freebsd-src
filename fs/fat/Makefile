#
# Makefile for the Linux fat filesystem support.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := fat.o

export-objs := fatfs_syms.o

obj-y    := buffer.o cache.o dir.o file.o inode.o misc.o cvf.o fatfs_syms.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
