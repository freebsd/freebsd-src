# Generated automatically from Makefile.in by configure.
# Makefile for GNU Texindex.
# Copyright (C) 1990, 1991, 1992 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#### Start of system configuration section. ####

srcdir = .
VPATH  = $(srcdir):$(common)

common = $(srcdir)/../libtxi

CC = gcc
INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

LN	= ln
RM	= rm -f
TAR	= tar
MKDIR	= mkdir

DEFS =  -DSTDC_HEADERS=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_VARARGS_H=1 -DHAVE_SYS_TIME_H=1 -DHAVE_SYS_FCNTL_H=1 -DHAVE_SETVBUF=1 -DHAVE_GETCWD=1 -DHAVE_BZERO=1 -DHAVE_RINDEX=1 -DHAVE_VFPRINTF=1 -DHAVE_VSPRINTF=1
LIBS = -L../libtxi -ltxi
LOADLIBES = $(LIBS)

SHELL = /bin/sh

CFLAGS = -g
LDFLAGS = -g

prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
# Prefix for each installed program, normally empty or `g'.
binprefix =
libdir = $(prefix)/lib
# Prefix for each installed man page, normally empty or `g'.
manprefix =
mandir = $(prefix)/man/man1
manext = 1
infodir = $(prefix)/info

#### End of system configuration section. ####

all: texindex
sub-all: all

.c.o:
	$(CC) -c $(CPPFLAGS) $(DEFS) -I. -I$(srcdir) -I$(common) $(CFLAGS) $<


install: all
	$(INSTALL_PROGRAM) texindex $(bindir)/texindex
	$(INSTALL_PROGRAM) $(srcdir)/texi2dvi $(bindir)/texi2dvi

uninstall:
	rm -f $(bindir)/texindex $(bindir)/texi2dvi

Makefile: Makefile.in ../config.status
	cd ..; sh config.status

TAGS:
	etags *.c *.h $(common)/getopt*.c $(common)/getopt.h

clean:
	rm -f *.o a.out core core.* texindex

mostlyclean: clean

distclean: clean
	rm -f Makefile config.status

realclean: distclean
	rm -f TAGS

texindex: texindex.o ../libtxi/libtxi.a
	$(CC) $(LDFLAGS) -o texindex texindex.o $(LOADLIBES)

texindex.o: texindex.c $(common)/getopt.h

# Prevent GNU make v3 from overflowing arg limit on SysV.
.NOEXPORT:
