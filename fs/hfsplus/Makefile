#
# Makefile for the linux hfsplus filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET :=	hfsplus.o

obj-y    :=	super.o options.o inode.o extents.o catalog.o dir.o btree.o \
		bnode.o brec.o bfind.o tables.o unicode.o wrapper.o

obj-m	 :=	$(O_TARGET)

include $(TOPDIR)/Rules.make
