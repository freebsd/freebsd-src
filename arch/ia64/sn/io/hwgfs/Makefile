#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# Makefile for the sn2 io routines.

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

O_TARGET := snio.o

export-objs := hcl.o ramfs.o interface.o

obj-y				+= hcl.o labelcl.o hcl_util.o invent_stub.o \
				   ramfs.o interface.o

include $(TOPDIR)/Rules.make
