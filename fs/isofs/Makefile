#
# Makefile for the Linux isofs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := isofs.o

obj-y  := namei.o inode.o dir.o util.o rock.o
obj-$(CONFIG_JOLIET) += joliet.o
obj-$(CONFIG_ZISOFS) += compress.o

obj-m  := $(O_TARGET)

CFLAGS_compress.o := -I $(TOPDIR)/fs/inflate_fs

include $(TOPDIR)/Rules.make
