#
# Makefile for the Zorro bus specific drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#

O_TARGET := driver.o

export-objs		:= zorro.o

obj-$(CONFIG_ZORRO)	+= zorro.o names.o
obj-$(CONFIG_PROC_FS)	+= proc.o

include $(TOPDIR)/Rules.make

names.o: names.c devlist.h

devlist.h: zorro.ids gen-devlist
	./gen-devlist <zorro.ids

gen-devlist: gen-devlist.c
	$(HOSTCC) $(HOSTCFLAGS) -o gen-devlist gen-devlist.c
