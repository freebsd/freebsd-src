#
# Makefile for the linux ppc-specific parts of the memory manager.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

EXTRA_CFLAGS = -mno-minimal-toc

O_TARGET := mm.o

obj-y           := fault.o init.o extable.o imalloc.o

include $(TOPDIR)/Rules.make
