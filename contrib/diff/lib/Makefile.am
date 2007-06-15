# Automakefile for GNU Diffutils library.

# Copyright (C) 2001, 2002, 2004 Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

noinst_LIBRARIES = libdiffutils.a

noinst_HEADERS = cmpbuf.h prepargs.h quotesys.h

libdiffutils_a_SOURCES = $(lib_SOURCES)
lib_SOURCES = cmpbuf.c prepargs.c quotesys.c

libdiffutils_a_LIBADD = @ALLOCA@ @LIBOBJS@
libdiffutils_a_DEPENDENCIES = $(libdiffutils_a_LIBADD)

BUILT_SOURCES =
DISTCLEANFILES =
EXTRA_DIST = setmode.h waitpid.c
MOSTLYCLEANFILES =

include gnulib.mk
