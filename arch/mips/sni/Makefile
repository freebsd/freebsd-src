#
# Makefile for the SNI specific part of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

all:	sni.o int-handler.o

O_TARGET := sni.o

obj-y	 		+= int-handler.o io.o irq.o pcimt_scache.o reset.o \
			   setup.o

obj-$(CONFIG_PCI)	+= pci.o

int-handler.o: int-handler.S

include $(TOPDIR)/Rules.make
