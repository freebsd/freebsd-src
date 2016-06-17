#
# Makefile for the Linux IrDA IrNET protocol layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

MOD_LIST_NAME := IRDA_MODULES
O_TARGET := irnet.o

obj-y	:= irnet_ppp.o irnet_irda.o
obj-m	:= $(O_TARGET)

include $(TOPDIR)/Rules.make
