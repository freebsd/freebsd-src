## Process this file with automake to produce Makefile.in
# Makefile for GNU CVS contributed sources.
# Do not use this makefile directly, but only from `../Makefile'.
# Copyright (C) 1986, 1988-1990 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

contribdir = $(pkgdatadir)/contrib

contrib_SCRIPTS = \
	check_cvs \
	clmerge \
	cln_hist \
	commit_prep \
	cvs2vendor \
	cvs_acls \
	cvscheck \
	debug_check_log \
	log \
	log_accum \
	mfpipe \
	pvcs2rcs \
	rcs-to-cvs \
	rcs2log \
	rcslock \
	sccs2rcs

contrib_DATA = \
	README \
	intro.doc

contrib_MANS = \
	cvscheck.man

bin_LINKS = \
	rcs2log

EXTRA_DIST = \
	.cvsignore \
	$(contrib_DATA) \
	$(contrib_MANS) \
	cvs2vendor.sh \
	cvscheck.sh \
	cvshelp.man \
	debug_check_log.sh \
	descend.sh \
	descend.man \
	dirfns.shar \
	rcs-to-cvs.sh \
	rcs2log.sh \
	rcs2sccs.sh

CLEANFILES = $(bin_SCRIPTS) $(contrib_SCRIPTS)

# we'd rather have a link here rather than two copies of a script
install-data-local:
	: FIXME - this path should be determined dynamically from bindir
	: and contribdir
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	@list='$(bin_LINKS)'; for p in $$list; do \
	    echo "test ! -e $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'`"; \
	    echo "  && cd $(DESTDIR)$(bindir) && $(LN_S) ../share/$(PACKAGE)/contrib/`echo $$p|sed '$(transform)'` ."; \
	    (test ! -e $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'` \
	        && cd $(DESTDIR)$(bindir) && $(LN_S) ../share/$(PACKAGE)/contrib/`echo $$p|sed '$(transform)'` .) \
	      || (echo "Link creation failed" && if test -f $$p; then \
	           echo " $(INSTALL_SCRIPT) $$p $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'`"; \
	           $(INSTALL_SCRIPT) $$p $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'`; \
	         else if test -f $(srcdir)/$$p; then \
	           echo " $(INSTALL_SCRIPT) $(srcdir)/$$p $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'`"; \
	           $(INSTALL_SCRIPT) $(srcdir)/$$p $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'`; \
	         else :; fi; fi); \
	done

uninstall-local:
	@$(NORMAL_UNINSTALL)
	list='$(bin_LINKS)'; for p in $$list; do \
	  rm -f $(DESTDIR)$(bindir)/`echo $$p|sed '$(transform)'`; \
	done

SUFFIXES = .sh

.sh:
	rm -f $@
	cp $< $@
	chmod +x $@

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
