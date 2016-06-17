#
#  Copyright 2000 RidgeRun, Inc.
#  Author: RidgeRun, Inc.
#     	glonnon@ridgerun.com, skranz@ridgerun.com, stevej@ridgerun.com
#
# Makefile for the Galileo EV64120 board.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

all:	ev64120.o int-handler.o

O_TARGET := ev64120.o

obj-y			+= serialGT.o int-handler.o promcon.o reset.o setup.o \
			   irq.o
obj-$(CONFIG_PCI)	+= fixup-ev64120.o

int-handler.o: int-handler.S

include $(TOPDIR)/Rules.make
