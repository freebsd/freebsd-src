#
# Makefile for the Riva framebuffer driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET := rivafb.o

obj-y    := fbdev.o riva_hw.o accel.o
obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
