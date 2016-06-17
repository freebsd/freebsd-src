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

export-objs := pci_dma.o iomv.o

obj-y				+= pci.o pci_dma.o pci_bus_cvlink.o iomv.o

include $(TOPDIR)/Rules.make
