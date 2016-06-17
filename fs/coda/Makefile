#
# Makefile for the Linux Coda filesystem routines.
#

O_TARGET := coda.o

obj-y   := psdev.o cache.o cnode.o inode.o dir.o file.o upcall.o coda_linux.o\
           symlink.o pioctl.o sysctl.o 
obj-m   := $(O_TARGET)

# If you want debugging output, please uncomment the following line.

# EXTRA_CFLAGS += -DDEBUG -DDEBUG_SMB_MALLOC=1

include $(TOPDIR)/Rules.make

