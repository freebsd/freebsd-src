## Process this file with automake to produce Makefile.in
# Makefile for GNU CVS documentation (excluding man pages - see ../man).
# Copyright (C) 1986, 1988-1990, 2000 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

info_TEXINFOS = cvs.texinfo cvsclient.texi

POSTSCRIPTS = \
	cvs.ps \
	cvs-paper.ps \
	cvsclient.ps

PDFS = \
	cvs-paper.pdf \
	cvs.pdf \
	cvsclient.pdf

TXTS = \
	cvs.txt \
	cvsclient.txt

EXTRA_DIST = \
	$(POSTSCRIPTS) \
	.cvsignore \
	ChangeLog.fsf \
	RCSFILES \
	mdate-sh \
	cvs-paper.ms

CLEANFILES = \
	$(PDFS) \
	$(TXTS)

MAINTAINERCLEANFILES = \
	cvs-paper.ps

doc ps: $(POSTSCRIPTS)
.PHONY: doc ps

pdf: $(PDFS)
.PHONY: pdf

txt: $(TXTS)
.PHONY: txt

dvi: cvs.dvi cvsclient.dvi
.PHONY: dvi

# FIXME-AUTOMAKE:
# For some reason if I remove version.texi, it doesn't get built automatically.
# This needs to be fixed in automake.
cvs.aux cvs.pdf cvs.txt: cvs.texinfo $(srcdir)/version.texi
cvsclient.aux cvsclient.pdf cvsclient.txt: cvsclient.texi

# These targets need to be very specific so that the other PDFs get generated
# correctly.  If they are more generic and cvs.ps is made before cvs.pdf, then
# cvs.pdf will be generated from the generic target and the PS source, which
# contains less information (hyperlinks and such) than the usual texinfo source.
cvs-paper.ps: cvs-paper.ms
	$(ROFF) -t -p -ms -Tps $< > $@-t
	mv $@-t $@

cvs-paper.pdf: cvs-paper.ps
	$(PS2PDF) $< $@

SUFFIXES = .aux .txt .pdf

# texinfo based targets automake neglects to include
.texinfo.pdf:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf --batch $<
.txi.pdf:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf --batch $<
.texi.pdf:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf --batch $<

.texinfo.txt:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(MAKEINFO) $< --no-headers -o $@
.txi.txt:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(MAKEINFO) $< --no-headers -o $@
.texi.txt:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(MAKEINFO) $< --no-headers -o $@

##
## MAINTAINER Targets
##

## realclean ##
# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
