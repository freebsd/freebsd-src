#
# Makefile for common code of NEC Osprey board
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := osprey.o

obj-y	 := setup.o prom.o reset.o

obj-$(CONFIG_KGDB)	+= dbg_io.o

include $(TOPDIR)/Rules.make
