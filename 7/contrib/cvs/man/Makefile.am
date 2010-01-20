## Process this file with automake to produce Makefile.in
# Makefile.am for GNU CVS man pages.
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

man_MANS = cvs.5 cvsbug.8
EXTRA_DIST = \
	.cvsignore \
	$(man_MANS)

# for backwards compatibility with the old makefiles
realclean: maintainer-clean
.PHONY: realclean
