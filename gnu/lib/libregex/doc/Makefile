# Generated automatically from Makefile.in by configure.
# Makefile for regex documentation.
# 
# Copyright (C) 1992 Free Software Foundation, Inc.
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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Installation directories.
prefix = /usr
infodir = $(prefix)/info

srcdir = .
VPATH = .:../.

INSTALL = /usr/bin/install -c
INSTALL_DATA = $(INSTALL) -m 644

MAKEINFO = makeinfo --no-split
SHELL = /bin/sh
TEX = tex
TEXINDEX = texindex

default all: regex.info regex.dvi
.PHONY: default all

# We need to include some code from regex.h.
regex.texi: xregex.texi
	rm -f $@
	gawk -f include.awk -vsource=../$(srcdir)/regex.h \
	  <../$(srcdir)/doc/xregex.texi \
          | expand >$@
	chmod a-w $@

regex.dvi: regex.cps
	$(TEX) regex.texi
regex.cps: regex.cp
	$(TEXINDEX) regex.??
regex.cp: regex.texi
	$(TEX) ../$(srcdir)/doc/regex.texi

regex.info: regex.texi
	$(MAKEINFO) ../$(srcdir)/doc/regex.texi

# I know of no way to make a good TAGS file from Texinfo source.
TAGS:

check:
.PHONY: check

install: regex.info
	-mkdir $(prefix) $(infodir)
	for i in *.info*; do $(INSTALL_DATA) $$i $(infodir)/$$i; done
.PHONY: install

clean mostlyclean:
	rm -f regex.?? *.dvi *.log *.toc

distclean: clean
	rm -f Makefile
	for f in regex.??s; do if test -z "`cat $$f`"; then rm -f $$f; fi; done

realclean: distclean
	rm -f *.info* regex.??? regex.texi TAGS

extraclean: distclean
	rm -f patch* *~* *\#* *.orig *.rej *.bak core a.out
.PHONY: mostlyclean clean distclean realclean extraclean

Makefile: Makefile.in ../config.status
	(cd ..; sh config.status)

# Prevent GNU make 3 from overflowing arg limit on system V.
.NOEXPORT:

# Assumes $(distdir) is the place to put our files.
distfiles = Makefile.in *.texi texinfo.tex include.awk \
            regex.info* regex.aux regex.cps
dist: Makefile regex.info regex.cps
	mkdir $(distdir)
	ln $(distfiles) $(distdir)
.PHONY: dist
