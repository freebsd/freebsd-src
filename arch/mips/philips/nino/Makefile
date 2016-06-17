#
# Makefile for the Philips Nino specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET	:= nino.o

all: nino.o

obj-y		:= int-handler.o irq.o setup.o prom.o power.o

int-handler.o: int-handler.S

clean:
		rm -f *.o

include $(TOPDIR)/Rules.make
