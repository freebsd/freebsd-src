#
# Makefile for the Linux nfs filesystem routines.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile.

O_TARGET := nfsd.o

obj-y :=    nfssvc.o nfsctl.o nfsproc.o nfsfh.o vfs.o \
	    export.o auth.o lockd.o nfscache.o nfsxdr.o \
	    stats.o

obj-$(CONFIG_NFSD_V3) += nfs3proc.o nfs3xdr.o

obj-m := $(O_TARGET)

include $(TOPDIR)/Rules.make
