#
# Makefile.am --- automake input file for gawk
#
# Copyright (C) 2000-2001 the Free Software Foundation, Inc.
#
# This file is part of GAWK, the GNU implementation of the
# AWK Programming Language.
#
# GAWK is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# GAWK is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
#

## process this file with automake to produce Makefile.in

# Automatic de-ANSI-fication if needed
AUTOMAKE_OPTIONS = ansi2knr

# This undocumented variable insures that aclocal runs
# correctly after changing configure.in
ACLOCAL_AMFLAGS = -I m4

# This insures that make flags get passed down to child makes.
AM_MAKEFLAGS = 'CFLAGS=$(CFLAGS)' 'LDFLAGS=$(LDFLAGS)'

# Stuff to include in the dist that doesn't need it's own
# Makefile.am files
EXTRA_DIST = \
	COPYING \
	FUTURES \
	INSTALL \
	LIMITATIONS \
	NEWS \
	POSIX.STD \
	PROBLEMS \
	README_d \
	bisonfix.sed \
	depcomp \
	extension \
	fixvers \
	m4 \
	missing \
	missing_d \
	pc \
	posix \
	unsupported \
	vms

# The order to do things in.
# Build in intl first in case we need the local gettext version.
# Build explicitly  "." next in order to build gawk first, so
# that `make check' without a prior `make' works.
SUBDIRS = \
	intl \
	. \
	awklib \
	doc \
	po \
	test

# what to make and install
bin_PROGRAMS = gawk pgawk

# sources for both gawk and pgawk
base_sources = \
	array.c \
	awk.h \
	awkgram.y \
	builtin.c \
	custom.h \
	dfa.c \
	dfa.h \
	ext.c \
	field.c \
	gawkmisc.c \
	getopt.c \
	getopt.h \
	getopt1.c \
	io.c \
	main.c \
	msg.c \
	node.c \
	patchlev.h \
	protos.h \
	random.c \
	random.h \
	re.c \
	regex.c \
	regex.h \
	replace.c \
	version.in \
	version.c

gawk_SOURCES = $(base_sources) eval.c profile.c
pgawk_SOURCES = $(base_sources) eval_p.c profile_p.c

# Get extra libs as needed
LDADD = @INTLLIBS@ @SOCKET_LIBS@

# stuff for compiling gawk/pgawk
DEFPATH="\".:$(datadir)\""

DEFS= -DDEFPATH=$(DEFPATH) -DHAVE_CONFIG_H -DGAWK -DLOCALEDIR="\"$(datadir)/locale\""

INCLUDES = -I. -I$(srcdir) -I$(srcdir)/intl

# We want hard links for install-exec-hook, below
LN= ln

# First, add a link from gawk to gawk-X.Y.Z
# For systems where gawk is awk, add a link to awk
install-exec-hook:
	(fullname=gawk-`./gawk --version | sed 1q | awk '{print $$3}'` ; \
	cd $(DESTDIR)$(bindir); \
	$(LN) gawk $$fullname ; \
	if [ ! -f awk ]; \
	then	$(LN_S) gawk awk; \
	fi; exit 0)

# Undo the above when uninstalling
uninstall-links:
	(cd $(DESTDIR)$(bindir); \
	if [ -f awk ] && cmp awk gawk > /dev/null; then rm -f awk; fi ; \
	fullname=gawk-`./gawk --version | sed 1q | ./gawk '{print $$3}'` ; \
	rm -f $$fullname; exit 0)

uninstall-recursive: uninstall-links

# force there to be a gawk executable before running tests
check-local: gawk pgawk

# A little extra clean up when making distributions.
# FIXME: most of this rule should go away upon switching to libtool.
dist-hook:
	cd $(distdir)/extension ; rm -f *.o *.so

# Special rules for individual files
awkgram.c: awkgram.y
	$(YACC) $(AM_YFLAGS) $(YFLAGS) $< && sed -f $(srcdir)/bisonfix.sed < y.tab.c > $*.c && $(RM) y.tab.c
	if test -f y.tab.h; then \
	if cmp -s y.tab.h $*.h; then rm -f y.tab.h; else mv y.tab.h $*.h; fi; \
	else :; fi

# these force version and patchlevel to be up to date based
# on what's in configure.in.  No-one else but me should have
# to use them. So there.
patchlev.h: configure.in
	./fixvers -p

version.c: configure.in version.in
	./fixvers -v
