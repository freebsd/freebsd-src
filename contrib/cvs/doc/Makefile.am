## Process this file with automake to produce Makefile.in
# Makefile for GNU CVS documentation (excluding man pages - see ../man).
# Copyright (C) 1986, 1987, 1988, 1989, 1990, 1991, 1992, 1993, 1994,
#               1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003
#               Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

info_TEXINFOS = cvs.texinfo cvsclient.texi
man_MANS = cvs.1

PSS = \
	cvs.ps \
	cvs-paper.ps \
	cvsclient.ps

PDFS = \
	cvs.pdf \
	cvs-paper.pdf \
	cvsclient.pdf

TXTS = \
	cvs.txt \
	cvsclient.txt

EXTRA_DIST = \
	$(PSS) \
	.cvsignore \
	ChangeLog.fsf \
	RCSFILES \
	mdate-sh \
	cvs.1 \
	cvs-paper.ms \
	cvs.man.header \
	cvs.man.footer

MOSTLYCLEANFILES = \
	$(TXTS)

# These Automake generates MOSTLYCLEAN targets for PostScripts genersted from
# TEXINFOS, but it shouldn't when those files are in EXTRA_DIST
MAINTAINERCLEANFILES = \
	$(PSS)

doc: info ps
.PHONY: doc

txt: $(TXTS)
.PHONY: txt

dvi: cvs.dvi cvsclient.dvi
.PHONY: dvi

# FIXME-AUTOMAKE:
# For some reason if I remove version.texi, it doesn't get built automatically.
# This needs to be fixed in automake.
cvs.txt: cvs.texinfo $(srcdir)/version.texi
cvsclient.txt: cvsclient.texi $(srcdir)/version-client.texi

# These targets need to be very specific so that the other PDFs get generated
# correctly.  If they are more generic and cvs.ps is made before cvs.pdf, then
# cvs.pdf will be generated from the generic target and the PS source, which
# contains less information (hyperlinks and such) than the usual texinfo source.
cvs-paper.ps: cvs-paper.ms
	$(ROFF) -t -p -ms -Tps $< > $@-t
	mv $@-t $@

cvs-paper.pdf: cvs-paper.ps
	ps2pdf $< $@

# Targets to build a man page from cvs.texinfo.
cvs.1: @MAINTAINER_MODE_TRUE@ mkman cvs.man.header cvs.texinfo cvs.man.footer
	./mkman $(srcdir)/cvs.man.header $(srcdir)/cvs.texinfo \
		$(srcdir)/cvs.man.footer >$@

# texinfo based targets automake neglects to include
SUFFIXES = .txt
.texinfo.txt:
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 --no-headers -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
.txi.txt:
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 --no-headers -o $@ `test -f '$<' || echo '$(srcdir)/'`$<
.texi.txt:
	$(MAKEINFO) $(AM_MAKEINFOFLAGS) $(MAKEINFOFLAGS) -I $(srcdir) \
	 --no-headers -o $@ `test -f '$<' || echo '$(srcdir)/'`$<

##
## MAINTAINER Targets
##

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
