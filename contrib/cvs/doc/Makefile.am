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

#DISTFILES = \
#	.cvsignore \
#	RCSFILES \

# AUTOMAKE_OPTIONS = no-texinfo.tex

# FIXME - these rebuild every 'make dist' with some BSD makes due to a bug
# that prevents make from searching VPATH for targets.  There is some
# machinery in place in configure.in to work around this for some targets
# (see the CVSvn.texi target below) now, but fixing the two info_TEXINFOS
# targets below is probably going to require work on Automake
info_TEXINFOS = cvs.texinfo cvsclient.texi
cvs_TEXINFOS = CVSvn.texi
cvsclient_TEXINFOS = CVSvn.texi

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
	CVSvn.texi.in \
	ChangeLog.fsf \
	RCSFILES \
	cvs-paper.ms \
	cvs.aux \
	cvsclient.aux

# Listing cvs.cps & cvs.fns here is a workaround for an Automake deficiency
MOSTLYCLEANFILES = \
	cvs.cps \
	cvs.fns

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

cvs.aux cvs.pdf cvs.txt: cvs.texinfo $(cvs_TEXINFOS)
cvsclient.aux cvsclient.pdf cvsclient.txt: cvsclient.texi $(cvsclient_TEXINFOS)

# These targets need to be very specific so that the other PDFs get generated correctly.
# If they are more generic and cvs.ps is made before cvs.pdf, then cvs.pdf will be
# generated from the generic target and the PS source, which contains less information
# than the usual texinfo source.
cvs-paper.pdf: cvs-paper.ps
	$(ROFF) -t -p -ms -Tps $< > $@-t
	mv $@-t $@

cvs-paper.ps: cvs-paper.ms
	$(PS2PDF) $< $@

SUFFIXES = .aux .txt .pdf

# texinfo based targets automake neglects to include
.texinfo.pdf:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf $<
.txi.pdf:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf $<
.texi.pdf:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) --pdf $<

.texinfo.aux:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) $<
.txi.aux:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) $<
.texi.aux:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(TEXI2DVI) $<

.texinfo.txt:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(MAKEINFO) $< --no-headers -o $@
.txi.txt:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(MAKEINFO) $< --no-headers -o $@
.texi.txt:
	TEXINPUTS=$(srcdir):$$TEXINPUTS \
	  MAKEINFO='$(MAKEINFO) -I $(srcdir)' $(MAKEINFO) $< --no-headers -o $@

## MAINTAINER Targets

## CVSvn.texi ##
# - CVSvn.texi needs to be updated only once, since it depends on
#   configure.in, not on the results of a 'configure' run.
# - It is guaranteed (with GNU Make) that when the version in configure.in
#   is changed, acversion.m4 is built only after the new version number is
#   propagated to the Makefile.  (Libtool uses the same guarantee.)
# - Of the next three targets, only the next one or the following two should be
#   enabled, not all three
if MAKE_TARGETS_IN_VPATH
# - This is the target for systems without the BSD VPATH bug in make or with
#   $(srcdir) = $(builddir)
CVSvn.texi: $(srcdir)/CVSvn.texi.in $(top_srcdir)/configure.in
	sed 's,@VERSION\@,$(VERSION),g' $(srcdir)/CVSvn.texi.in >$(srcdir)/CVSvn.ttexi
	mv $(srcdir)/CVSvn.ttexi $(srcdir)/CVSvn.texi
else
# - The next two targets are a workaround for the fact that some BSD makes
#   don't look through VPATH for targets, but only for sources.
# - They should be disabled on systems without the bug and in environments
#   where $(srcdir) = $(builddir)
CVSvn.texi: $(srcdir)/CVSvn.texi
	# use cat so the time stamp gets set properly
	cat $(srcdir)/CVSvn.texi >$@
$(srcdir)/CVSvn.texi: $(srcdir)/CVSvn.texi.in $(top_srcdir)/configure.in
	sed 's,@VERSION\@,$(VERSION),g' $(srcdir)/CVSvn.texi.in >$(srcdir)/CVSvn.ttexi
	mv $(srcdir)/CVSvn.ttexi $(srcdir)/CVSvn.texi
endif

## realclean ##
# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
