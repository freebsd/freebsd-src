#
# Makefile for the ia64-specific parts of the memory manager.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET := mm.o

export-objs := init.o

obj-y	 := init.o fault.o tlb.o extable.o
obj-$(CONFIG_NUMA) += numa.o
obj-$(CONFIG_DISCONTIGMEM) += discontig.o
obj-$(CONFIG_HUGETLB_PAGE) += hugetlbpage.o

include $(TOPDIR)/Rules.make
