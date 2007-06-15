# Automakefile for GNU diffutils man pages

# Copyright (C) 2002 Free Software Foundation, Inc.

## This program is free software; you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2, or (at your option)
## any later version.

## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.

## You should have received a copy of the GNU General Public License
## along with this program; if not, write to the Free Software
## Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
## 02111-1307, USA.

dist_man_MANS =	cmp.1 diff.1 diff3.1 sdiff.1
MAINTAINERCLEANFILES = $(dist_man_MANS)

S = $(top_srcdir)/src
cmp.1: $S/cmp.c
diff.1: $S/diff.c
diff3.1: $S/diff3.c
sdiff.1: $S/sdiff.c

# Depend on configure.ac to get version number changes.
$(dist_man_MANS): $(top_srcdir)/configure.ac
	base=`expr $@ : '\(.*\).1'` && \
	  (echo '[NAME]' && sed 's@/\* *@@; s/-/\\-/; q' $S/$$base.c) | \
	  $(HELP2MAN) -i - -S '$(PACKAGE) $(VERSION)' ../src/$$base | \
	  sed 's/^\.B info .*/.B info diff/' >$@
