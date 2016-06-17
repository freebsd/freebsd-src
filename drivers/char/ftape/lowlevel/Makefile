#
#       Copyright (C) 1996, 1997 Clau-Justus Heine.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
#
# $Source: /homes/cvs/ftape-stacked/ftape/lowlevel/Makefile,v $
# $Revision: 1.4 $
# $Date: 1997/10/07 09:26:02 $
#
#      Makefile for the lowlevel part QIC-40/80/3010/3020 floppy-tape
#      driver for Linux.
#

O_TARGET := ftape.o

export-objs := ftape_syms.o

obj-y     = ftape-init.o fdc-io.o fdc-isr.o \
	    ftape-bsm.o ftape-ctl.o ftape-read.o ftape-rw.o \
	    ftape-write.o ftape-io.o ftape-calibr.o ftape-ecc.o fc-10.o \
	    ftape-buffer.o ftape-format.o ftape_syms.o

obj-$(CONFIG_FTAPE) += ftape-setup.o

ifndef CONFIG_FT_NO_TRACE_AT_ALL
obj-y   += ftape-tracing.o
endif

ifeq ($(CONFIG_PROC_FS),y)
ifeq ($(CONFIG_FT_PROC_FS),y)
obj-y += ftape-proc.o
endif
endif

obj-m  = $(O_TARGET)

include $(TOPDIR)/Rules.make

