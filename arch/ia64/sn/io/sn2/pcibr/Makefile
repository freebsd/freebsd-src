#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# Makefile for the sn2 io pcibr routines.

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

EXTRA_CFLAGS    += -DSHUB_SWAP_WAR

O_TARGET := pcibr.o
export-objs := pcibr_intr.o

obj-y	+= pcibr_ate.o pcibr_config.o \
           pcibr_dvr.o pcibr_hints.o \
	   pcibr_intr.o pcibr_rrb.o pcibr_slot.o \
	   pcibr_error.o

include $(TOPDIR)/Rules.make
