#
# Makefile for Linux arch/m68k/mvme16x source directory
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := mvme16x.o

export-objs	:= mvme16x_ksyms.o

obj-y		:= config.o 16xints.o rtc.o mvme16x_ksyms.o

include $(TOPDIR)/Rules.make
