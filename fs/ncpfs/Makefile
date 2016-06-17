#
# Makefile for the linux ncp filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := ncpfs.o

obj-y    := dir.o file.o inode.o ioctl.o mmap.o ncplib_kernel.o sock.o \
		symlink.o ncpsign_kernel.o
obj-m    := $(O_TARGET)

# If you want debugging output, please uncomment the following line
# EXTRA_CFLAGS += -DDEBUG_NCP=1

CFLAGS_ncplib_kernel.o := -finline-functions

include $(TOPDIR)/Rules.make
