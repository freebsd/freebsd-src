## Makefile.am for GNU DIFF
## Copyright (C) 2001 Free Software Foundation, Inc.
##
## This file is part of GNU DIFF.
##
## GNU DIFF is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.
##
## GNU DIFF is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

INCLUDES = -I$(top_srcdir)/lib

noinst_LIBRARIES = libdiff.a

libdiff_a_SOURCES = diff.c diff3.c analyze.c cmpbuf.c cmpbuf.h io.c \
        context.c ed.c normal.c ifdef.c util.c dir.c version.c diff.h \
        side.c system.h diffrun.h

EXTRA_DIST = ChangeLog build_diff.com diagmeet.note libdiff.dsp
