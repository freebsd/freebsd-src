## Process this file with automake to produce Makefile.in
# Makefile for GNU CVS program.
# Copyright (C) 1986, 1988-1990, 2000 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

SHELL = /bin/sh

# $(includeopt) is CVS specific and set by configure
# FIXME - This includes line is dependant on its order.  This means there is
# some namespace hackery going on that maybe shouldn't be.  Long term fix is to
# try and remove naming ocnflicts and fix Automake to allow particular includes
# to be attached only to particular object files.  Short term fix is either or.
##INCLUDES = -I. -I.. -I$(srcdir) -I$(top_srcdir)/lib
INCLUDES = -I$(top_srcdir)/lib -I$(top_srcdir)/diff -I$(top_srcdir)/zlib $(includeopt)

bin_PROGRAMS = cvs
bin_SCRIPTS = cvsbug

# The cvs executable
cvs_SOURCES = \
	add.c \
	admin.c \
	annotate.c \
	buffer.c \
	checkin.c \
	checkout.c \
	classify.c \
	client.c \
	commit.c \
	create_adm.c \
	cvsrc.c diff.c \
	edit.c \
	entries.c \
	error.c \
	expand_path.c \
	fileattr.c \
	filesubr.c \
	find_names.c \
	hardlink.c \
	hash.c \
	history.c \
	ignore.c \
	import.c \
	lock.c \
	log.c \
	login.c \
	logmsg.c \
	main.c \
	mkmodules.c \
	modules.c \
	myndbm.c \
	no_diff.c \
	parseinfo.c \
	patch.c \
	rcs.c \
	rcscmds.c \
	recurse.c \
	release.c \
	remove.c \
	repos.c \
	root.c \
	run.c \
	scramble.c \
	server.c \
	status.c \
	subr.c \
	tag.c \
	update.c \
	vers_ts.c \
	watch.c \
	wrapper.c \
	zlib.c \
	buffer.h \
	client.h \
	cvs.h \
	edit.h \
	error.h \
	fileattr.h \
	hardlink.h \
	hash.h \
	myndbm.h \
	rcs.h \
	server.h \
	update.h \
	watch.h
cvs_LDADD = \
	../diff/libdiff.a \
	../lib/libcvs.a \
	../zlib/libz.a \
	version.o
cvs_EXTRA_DIST = version.c

# extra clean targets
# wish this could be distclean-hdr-local but it's not part of automake
DISTCLEANFILES = options.h-SAVED check.log check.plog

# General
EXTRA_DIST = \
	$(cvs_EXTRA_DIST) \
	.cvsignore \
	ChangeLog-9194 \
	ChangeLog-9395 \
	ChangeLog-96 \
	ChangeLog-97 \
	build_src.com \
	sanity.sh \
	version.c \
	version.c.in

check-local:
	$(SHELL) $(srcdir)/sanity.sh `pwd`/cvs

.PHONY: remotecheck
remotecheck: all
	$(SHELL) $(srcdir)/sanity.sh -r `pwd`/cvs

## MAINTAINER Targets

# version.c
# - build this here so that we can distribute it
# - version.c needs to be updated only once, since it depends on
#   configure.in, not on the results of a 'configure' run.
# - It is guaranteed (with GNU Make) that when the version in configure.in
#   is changed, acversion.m4 is built only after the new version number is
#   propagated to the Makefile.  (Libtool uses the same guarantee.)
# - need the explicit version.o dependency or else make won't match
#   $(srcdir)/version.c when looking for a dependency for version.c
version.o: $(srcdir)/version.c
$(srcdir)/version.c: $(srcdir)/version.c.in $(top_srcdir)/configure.in
	sed 's,@VERSION\@,$(VERSION),g' $(srcdir)/version.c.in >$(srcdir)/version.tc
	mv $(srcdir)/version.tc $(srcdir)/version.c

# for backwards compatibility with the old makefiles
.PHONY: realclean
realclean: maintainer-clean
