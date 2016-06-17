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

obj-y				+= ioconfig_bus.o ifconfig_net.o

obj-$(CONFIG_PCIBA)		+= pciba.o

include $(TOPDIR)/Rules.make
