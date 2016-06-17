#
# Makefile for Linux arch/m68k/atari source directory
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := atari.o

export-objs	:= atari_ksyms.o

obj-y		:= config.o time.o debug.o ataints.o stdma.o \
			atasound.o stram.o atari_ksyms.o

obj-$(CONFIG_VT)	+= atakeyb.o joystick.o

ifeq ($(CONFIG_PCI),y)
obj-$(CONFIG_HADES)	+= hades-pci.o
endif

include $(TOPDIR)/Rules.make
