## Process this file with automake to produce Makefile.in
# Makefile for GNU CVS documentation (excluding man pages - see ../man).
#
# Copyright (C) 1986-2005 The Free Software Foundation, Inc.
#
# Portions Copyright (C) 1998-2005 Derek Price, Ximbiot <http://ximbiot.com>,
#                                  and others.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

info_TEXINFOS = cvs.texinfo cvsclient.texi
man_MANS = $(srcdir)/cvs.1

PSS = \
	cvs.ps \
	cvs-paper.ps \
	cvsclient.ps

PDFS = \
	cvs.pdf \
	$(srcdir)/cvs-paper.pdf \
	cvsclient.pdf

TXTS = \
	cvs.txt \
	cvsclient.txt

EXTRA_DIST = \
	.cvsignore \
	ChangeLog.fsf \
	RCSFILES \
	mdate-sh \
	$(srcdir)/cvs.1 \
	cvs-paper.ms \
	cvs.man.header \
	cvs.man.footer \
	$(PDFS)

MOSTLYCLEANFILES =

CLEANFILES = \
	$(PSS) \
	$(TXTS)

MAINTAINERCLEANFILES = \
	$(PDFS) \
	$(srcdir)/cvs.1

doc: info pdf
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

# The cvs-paper.pdf target needs to be very specific so that the other PDFs get
# generated correctly.  If a more generic .ps.pdf implicit target is defined,
# and cvs.ps is made before cvs.pdf, then cvs.pdf can be generated from the
# .ps.pdf target and the PS source, which contains less information (hyperlinks
# and such) than the usual texinfo source.
#
# It is possible that an implicit .ms.ps target could be safely defined.  I
# don't recall looking into it.
cvs-paper.ps: cvs-paper.ms
	$(ROFF) -t -p -ms -Tps $(srcdir)/cvs-paper.ms >cvs-paper.ps-t
	cp cvs-paper.ps-t $@
	-@rm -f cvs-paper.ps-t

# This rule introduces some redundancy, but `make distcheck' requires that
# Nothing in $(srcdir) be rebuilt, and this will always be rebuilt when it
# is dependant on cvs-paper.ps and cvs-paper.ps isn't distributed.
$(srcdir)/cvs-paper.pdf: cvs-paper.ms
	$(ROFF) -t -p -ms -Tps $(srcdir)/cvs-paper.ms >cvs-paper.ps-t
	ps2pdf cvs-paper.ps-t cvs-paper.pdf-t
	cp cvs-paper.pdf-t $@
	-@rm -f cvs-paper.pdf-t cvs-paper.ps-t

MOSTLYCLEANFILES += cvs-paper.pdf-t cvs-paper.ps-t

# Targets to build a man page from cvs.texinfo.
$(srcdir)/cvs.1: @MAINTAINER_MODE_TRUE@ mkman cvs.man.header cvs.texinfo cvs.man.footer
	$(PERL) ./mkman $(srcdir)/cvs.man.header $(srcdir)/cvs.texinfo \
		$(srcdir)/cvs.man.footer >cvs.tmp
	cp cvs.tmp $(srcdir)/cvs.1
	-@rm -f cvs.tmp

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
