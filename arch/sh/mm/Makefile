#
# Makefile for the Linux SuperH-specific parts of the memory manager.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := mm.o
obj-y	 := init.o fault.o extable.o clear_page.o copy_page.o

obj-$(CONFIG_PCI_SD0001) += ioremap.o
obj-$(CONFIG_CPU_SH3) += cache-sh3.o
obj-$(CONFIG_CPU_SH4) += cache-sh4.o ioremap.o

USE_STANDARD_AS_RULE := true

include $(TOPDIR)/Rules.make
