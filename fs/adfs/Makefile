#
# Makefile for the linux adfs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := adfs.o

obj-y    := dir.o dir_f.o dir_fplus.o file.o inode.o map.o super.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
