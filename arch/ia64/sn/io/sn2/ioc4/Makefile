#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2003 Silicon Graphics, Inc.  All Rights Reserved.
#
# Makefile for the sn2 io ioc4 routines.

EXTRA_CFLAGS    := -DLITTLE_ENDIAN

ifdef CONFIG_IA64_SGI_SN2
EXTRA_CFLAGS    += -DSHUB_SWAP_WAR
endif

O_TARGET := ioc4_if.o
export-objs := ioc4.o  sio_ioc4.o


obj-$(CONFIG_IA64_SGI_SN2)	+= sio_ioc4.o ioc4.o

include $(TOPDIR)/Rules.make
