#
# Makefile for the Linux hfs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := hfs.o

obj-y    := balloc.o bdelete.o bfind.o bins_del.o binsert.o bitmap.o bitops.o \
	    bnode.o brec.o btree.o catalog.o dir.o dir_cap.o dir_dbl.o \
	    dir_nat.o extent.o file.o file_cap.o file_hdr.o inode.o mdb.o \
            part_tbl.o string.o super.o sysdep.o trans.o version.o

obj-m    := $(O_TARGET)

include $(TOPDIR)/Rules.make
