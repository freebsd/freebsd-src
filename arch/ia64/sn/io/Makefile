#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2000-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# Makefile for the sn kernel routines.

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

EXTRA_CFLAGS    += -DSHUB_SWAP_WAR

O_TARGET := sgiio.o
 
export-objs := snia_if.o

obj-y :=	sgi_if.o xswitch.o cdl.o io.o snia_if.o

obj-y		+= sn2/snio.o machvec/snio.o \
	           drivers/snio.o platform_init/snio.o

ifdef CONFIG_HWGFS_FS
obj-$(CONFIG_HWGFS_FS)		+= hwgfs/snio.o
else
obj-$(CONFIG_DEVFS_FS)		+= hwgdfs/snio.o
endif

subdir-y			+= sn2 machvec drivers platform_init

ifdef CONFIG_HWGFS_FS
subdir-$(CONFIG_HWGFS_FS)	+= hwgfs
else
subdir-$(CONFIG_DEVFS_FS)	+= hwgdfs
endif

include $(TOPDIR)/Rules.make
