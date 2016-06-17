#
# Makefile for the linux cramfs routines.
#

O_TARGET := cramfs.o

obj-y  := inode.o uncompress.o

obj-m := $(O_TARGET)

CFLAGS_uncompress.o := -I $(TOPDIR)/fs/inflate_fs

include $(TOPDIR)/Rules.make
