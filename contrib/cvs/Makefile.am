## Process this file with automake to produce Makefile.in
# Master Makefile for the GNU Concurrent Versions System.
# Copyright (C) 1986, 1988-1992, 1994, 2000 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

AUTOMAKE_OPTIONS = 1.4a dist-bzip2

## Subdirectories to run make in for the primary targets.
# Unix source subdirs, where we'll want to run lint and etags:
# This is a legacy variable from b4 Automake
USOURCE_SUBDIRS = lib zlib diff src
# All other subdirs:
SUBDIRS = $(USOURCE_SUBDIRS) man doc contrib tools \
	windows-NT os2 emx vms

EXTRA_DIST = \
	.cvsignore \
	BUGS \
	ChangeLog.zoo \
	DEVEL-CVS \
	FAQ \
	HACKING \
	MINOR-BUGS \
	PROJECTS \
	README.VMS \
	TESTS \
	build.com \
	cvs-format.el \
	cvs.spec \
	cvs.spec.in \
	cvsnt.dsp \
	cvsnt.dsw \
	cvsnt.mak

.PHONY: remotecheck
remotecheck: all
	cd src && $(MAKE) $(AM_MAKEFLAGS) "$@"

.PHONY: doc
doc:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) "$@"

## MAINTAINER Targets

## cvs.spec  ##
# - cvs.spec needs to be updated only once, since it depends on
#   configure.in, not on the results of a 'configure' run.
# - It is guaranteed (with GNU Make) that when the version in configure.in
#   is changed, acversion.m4 is built only after the new version number is
#   propagated to the Makefile.  (Libtool uses the same guarantee.)
cvs.spec: $(srcdir)/cvs.spec.in $(top_srcdir)/configure.in
	sed 's,@VERSION\@,$(VERSION),g' $(srcdir)/cvs.spec.in >cvs.tspec
	mv cvs.tspec $(srcdir)/cvs.spec

# for backwards compatibility with the old makefiles
.PHONY: realclean
realclean: maintainer-clean
