#
# Makefile for the Linux minix filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := minix.o

obj-y    := bitmap.o itree_v1.o itree_v2.o namei.o inode.o file.o dir.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
