#
# Makefile for the linux ipc.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := ipc.o

obj-y   := util.o

obj-$(CONFIG_SYSVIPC) += msg.o sem.o shm.o

include $(TOPDIR)/Rules.make
