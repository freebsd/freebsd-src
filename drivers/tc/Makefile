#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

# All of the (potential) objects that export symbols.
# This list comes from 'grep -l EXPORT_SYMBOL *.[hc]'.

export-objs	:= tc.o

# Object file lists.

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

obj-$(CONFIG_TC) += tc.o
obj-$(CONFIG_ZS) += zs.o
obj-$(CONFIG_VT) += lk201.o lk201-map.o lk201-remap.o

# Files that are both resident and modular: remove from modular.

obj-m		:= $(filter-out $(obj-y), $(obj-m))

# Translate to Rules.make lists.

L_TARGET	:= tc.a

L_OBJS		:= $(sort $(filter-out $(export-objs), $(obj-y)))
LX_OBJS		:= $(sort $(filter     $(export-objs), $(obj-y)))
M_OBJS		:= $(sort $(filter-out $(export-objs), $(obj-m)))
MX_OBJS		:= $(sort $(filter     $(export-objs), $(obj-m)))

include $(TOPDIR)/Rules.make

lk201-map.c: lk201-map.map
	loadkeys --mktable lk201-map.map > lk201-map.c
