#
# Makefile for common code of gt64120-based boards.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := gt64120.o

obj-y	 		+= time.o
obj-$(CONFIG_PCI)	+= pci.o

include $(TOPDIR)/Rules.make
