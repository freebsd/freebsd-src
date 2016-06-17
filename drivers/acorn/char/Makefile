#
# Makefile for the kernel character device drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now inherited from the
# parent makes..
#

O_TARGET 	:= acorn-char.o

# Object file lists.

obj-y   	:=
obj-m		:=
obj-n		:=
obj-		:=

# All the objects that export symbols.
export-objs	:= mouse_rpc.o

obj-arc		:= keyb_arc.o
obj-rpc		:= keyb_ps2.o
obj-clps7500	:= keyb_ps2.o defkeymap-acorn.o
obj-riscstation := keyb_ps2.o defkeymap-acorn.o

obj-$(CONFIG_7K5MOUSE)		+= mouse_ps2.o
obj-$(CONFIG_RPCMOUSE)		+= mouse_rpc.o
obj-$(CONFIG_ATOMWIDE_SERIAL)	+= serial-atomwide.o
obj-$(CONFIG_DUALSP_SERIAL)	+= serial-dualsp.o
obj-$(CONFIG_ARCH_ACORN)	+= defkeymap-acorn.o i2c.o pcf8583.o
obj-$(CONFIG_L7200_KEYB)	+= defkeymap-l7200.o keyb_l7200.o

# Do the i2c and rtc last
obj-y		+= $(obj-$(MACHINE))

include $(TOPDIR)/Rules.make

%.c: %.map
	loadkeys --mktable $< > $@
