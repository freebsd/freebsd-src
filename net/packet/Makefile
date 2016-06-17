#
# Makefile for the packet AF.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := packet.o

obj-$(CONFIG_PACKET) += af_packet.o

include $(TOPDIR)/Rules.make
