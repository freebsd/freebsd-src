#
# awklib/Makefile.am --- automake input file for gawk
#
# Copyright (C) 1995-2001 the Free Software Foundation, Inc.
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

EXTRA_DIST = ChangeLog extract.awk eg stamp-eg

datadir = @datadir@/awk
libexecdir = @libexecdir@/awk

bin_SCRIPTS = igawk
libexec_PROGRAMS = pwcat grcat
AUXAWK = passwd.awk group.awk
nodist_grcat_SOURCES = grcat.c
nodist_pwcat_SOURCES = pwcat.c

all: stamp-eg $(AUXPROGS) igawk $(AUXAWK)

install-exec-hook: $(AUXAWK)
	$(mkinstalldirs) $(DESTDIR)$(datadir)
	for i in $(AUXAWK) $(srcdir)/eg/lib/*.awk ; do \
		progname=`echo $$i | sed 's;.*/;;'` ; \
		$(INSTALL_DATA) $$i $(DESTDIR)$(datadir)/$$progname ; \
	done

# libexecdir and datadir are removed in the top level Makefile's uninstall
uninstall-local:
	rm -fr $(DESTDIR)$(libexecdir)/* $(DESTDIR)$(datadir)/*
	rm -f $(DESTDIR)$(bindir)/igawk

clean-local:
	rm -f $(AUXAWK) igawk

stamp-eg: $(srcdir)/../doc/gawk.texi $(srcdir)/../doc/gawkinet.texi
	rm -fr eg stamp-eg
	$(AWK) -f $(srcdir)/extract.awk $(srcdir)/../doc/gawk.texi $(srcdir)/../doc/gawkinet.texi
	@echo 'some makes are stupid and will not check a directory' > stamp-eg
	@echo 'against a file, so this file is a place holder. gack.' >> stamp-eg

pwcat: $(srcdir)/eg/lib/pwcat.c
	$(CC) $(CFLAGS) $(srcdir)/eg/lib/pwcat.c $(LDFLAGS) -o $@

grcat: $(srcdir)/eg/lib/grcat.c
	$(CC) $(CFLAGS) $(srcdir)/eg/lib/grcat.c $(LDFLAGS) -o $@

igawk: $(srcdir)/eg/prog/igawk.sh
	cp $(srcdir)/eg/prog/igawk.sh $@ ; chmod 755 $@

passwd.awk: $(srcdir)/eg/lib/passwdawk.in
	(cd $(srcdir)/eg/lib ; \
	sed 's;/usr/local/libexec/awk;$(libexecdir);' < passwdawk.in) > passwd.awk

group.awk: $(srcdir)/eg/lib/groupawk.in
	(cd $(srcdir)/eg/lib ; \
	sed 's;/usr/local/libexec/awk;$(libexecdir);' < groupawk.in) > group.awk
