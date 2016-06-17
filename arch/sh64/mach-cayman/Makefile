#
# Makefile for the Hitachi Cayman specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_TARGET := cayman.o

obj-y := setup.o irq.o
obj-$(CONFIG_HEARTBEAT)	+= led.o

include $(TOPDIR)/Rules.make

