# Makefile,v 1.2 1992/04/07 20:49:07 berliner Exp
# Makefile for pcl-cvs release 1.02.
# Copyright (C) 1992  Per Cederqvist
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# This is the directory in which the ELFILES and ELCFILES will be
# installed.

lispdir = /usr/local/lib/elisp

# Where to install the info file.

prefix=/usr/local
infodir = $(prefix)/info

#
# The rest of this file should not need to be modified.
#

# Just in case...
SHELL = /bin/sh

ELFILES = pcl-cvs.el cookie.el elib-dll.el elib-node.el
ELCFILES = pcl-cvs.elc cookie.elc elib-dll.elc elib-node.elc
INFOFILES = pcl-cvs
TEXTMPS = pcl-cvs.aux pcl-cvs.log pcl-cvs.toc pcl-cvs.dvi pcl-cvs.cp \
	pcl-cvs.fn pcl-cvs.vr pcl-cvs.tp pcl-cvs.ky pcl-cvs.pg \
	pcl-cvs.cps pcl-cvs.fns pcl-cvs.kys pcl-cvs.pgs pcl-cvs.tps \
	pcl-cvs.vrs

INSTALL = install
INSTALL_DATA = $(INSTALL)

elcfiles:
	emacs -batch -l ./compile-all.el -f compile-pcl-cvs

all: elcfiles info

# Don't install the info file yet, since it requires makeinfo 
# version 2.something (and version 1.something is distributed with emacs).
#
# install: install_elc install_info
install: install_elc
	for i in $(ELFILES); do $(INSTALL_DATA) $$i $(lispdir)/$$i; done

install_elc: elcfiles
	for i in $(ELCFILES); do $(INSTALL_DATA) $$i $(lispdir)/$$i; done

install_info: pcl-cvs
	$(INSTALL_DATA) pcl-cvs $(infodir)/pcl-cvs

info pcl-cvs: pcl-cvs.texinfo
	makeinfo +fill-column=70 pcl-cvs.texinfo

pcl-cvs.dvi: pcl-cvs.texinfo
	tex pcl-cvs.texinfo
	-texindex pcl-cvs.cp pcl-cvs.fn pcl-cvs.vr pcl-cvs.tp pcl-cvs.ky \
		pcl-cvs.pg
	tex pcl-cvs.texinfo

mostlyclean clean realclean:
	rm -f *~ core $(ELCFILES) $(INFOFILES) $(TEXTMPS)

tags TAGS:
	etags *.el
