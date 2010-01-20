## Process this file with automake to produce Makefile.in
# Master Makefile for the GNU Concurrent Versions System.
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
	cvsnt.dep \
	cvsnt.dsp \
	cvsnt.dsw \
	cvsnt.mak \
	cvs.spec \
	mktemp.sh


## MAINTAINER Targets

.PHONY: localcheck remotecheck
localcheck remotecheck: all
	cd src && $(MAKE) $(AM_MAKEFLAGS) "$@"

.PHONY: doc
doc:
	cd doc && $(MAKE) $(AM_MAKEFLAGS) "$@"

# for backwards compatibility with the old makefiles
.PHONY: realclean
realclean: maintainer-clean
