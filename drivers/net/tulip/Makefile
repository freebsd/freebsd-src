#
# Makefile for the Tulip ethernet driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := tulip.o

obj-y   := eeprom.o interrupt.o media.o \
	   timer.o tulip_core.o		\
	   21142.o pnic.o pnic2.o
obj-m   := $(O_TARGET)

include $(TOPDIR)/Rules.make
