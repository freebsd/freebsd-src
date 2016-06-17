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

obj-y				+= ml_SN_intr.o shub_intr.o shuberror.o shub.o \
				   bte_error.o pcibr/pcibr.o pic.o geo_op.o \
				   l1_command.o klconflib.o klgraph.o \
				   ml_SN_init.o ml_iograph.o module.o pciio.o \
				   xbow.o xtalk.o shubio.o ioc4/ioc4_if.o

obj-$(CONFIG_KDB)		+= kdba_io.o

subdir-y			+= pcibr ioc4

include $(TOPDIR)/Rules.make
