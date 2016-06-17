#
# Makefile for Linux arch/m68k/mac source directory
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := mac.o

export-objs	:= mac_ksyms.o

obj-y		:= config.o bootparse.o macints.o iop.o via.o oss.o psc.o \
			baboon.o macboing.o debug.o misc.o mac_ksyms.o

include $(TOPDIR)/Rules.make
