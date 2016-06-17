#
# Makefile for Linux arch/m68k/amiga source directory
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := amiga.o

export-objs	:= amiga_ksyms.o

obj-y		:= config.o amiints.o cia.o chipram.o amisound.o amiga_ksyms.o

obj-$(CONFIG_AMIGA_PCMCIA)	+= pcmcia.o

include $(TOPDIR)/Rules.make
