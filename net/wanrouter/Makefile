#
# Makefile for the Linux WAN router layer.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := wanrouter.o

export-objs := wanmain.o

obj-y :=  wanproc.o wanmain.o
obj-m := $(O_TARGET)
ifneq ($(CONFIG_VENDOR_SANGOMA),n)
	obj-m += $(O_TARGET)
endif

include $(TOPDIR)/Rules.make
