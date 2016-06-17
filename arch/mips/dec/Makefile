#
# Makefile for the DECstation family specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := dec.o

all: dec.o

export-objs	:= rtc-dec.o setup.o wbflush.o

obj-y		:= ecc-berr.o int-handler.o ioasic-irq.o kn02-irq.o reset.o \
		   rtc-dec.o setup.o time.o

obj-$(CONFIG_PROM_CONSOLE)	+= promcon.o
obj-$(CONFIG_CPU_HAS_WB)	+= wbflush.o

int-handler.o:	int-handler.S

include $(TOPDIR)/Rules.make
