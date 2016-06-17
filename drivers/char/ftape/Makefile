#
#       Copyright (C) 1997 Claus Heine.
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
# $Source: /homes/cvs/ftape-stacked/ftape/Makefile,v $
# $Revision: 1.4 $
# $Date: 1997/10/05 19:17:56 $
#
#      Makefile for the QIC-40/80/3010/3020 floppy-tape driver for
#      Linux.
#

O_TARGET := ftape.o

subdir-$(CONFIG_FTAPE) += lowlevel
subdir-$(CONFIG_ZFTAPE) += zftape
subdir-$(CONFIG_ZFT_COMPRESSOR) += compressor

ifeq ($(CONFIG_FTAPE),y)
  obj-y    += lowlevel/ftape.o
endif

ifeq ($(CONFIG_ZFTAPE),y)
  obj-y   += zftape/zftape.o
endif

ifeq ($(CONFIG_ZFT_COMPRESSOR),y)
  obj-y += compressor/zft-compressor.o
endif

include $(TOPDIR)/Rules.make
