#Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.
#     Written by James Clark (jjc@jclark.com)
#
#This file is part of groff.
#
#groff is free software; you can redistribute it and/or modify it under
#the terms of the GNU General Public License as published by the Free
#Software Foundation; either version 2, or (at your option) any later
#version.
#
#groff is distributed in the hope that it will be useful, but WITHOUT ANY
#WARRANTY; without even the implied warranty of MERCHANTABILITY or
#FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#for more details.
#
#You should have received a copy of the GNU General Public License along
#with groff; see the file COPYING.  If not, write to the Free Software
#Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# This is set up so you can do:
#   make -f generate/Makefile
# in the parent directory of the directory containing this file.

srcdir=.
tfmdir=/usr/local/lib/groff-tfm
gfdir=/usr/local/lib/groff-gf

TFMTODIT=tfmtodit
FONTS=R I B BI CW MI S EX H HI HB SA SB
SPECIALFLAG=-s

all: $(FONTS)

# R is special because it contains \(pl \(eq

R: $(srcdir)/texr.map
	$(TFMTODIT) $(SPECIALFLAG) -g $(gfdir)/cmr10.300gf \
	  $(tfmdir)/cmr10.tfm $(srcdir)/texr.map $@

# I is special because it contains \(Po

I: $(srcdir)/texi.map
	$(TFMTODIT) $(SPECIALFLAG) -g $(gfdir)/cmti10.300gf \
	  $(tfmdir)/cmti10.tfm $(srcdir)/texi.map $@

B: $(srcdir)/texb.map
	$(TFMTODIT) -g $(gfdir)/cmbx10.300gf \
	  $(tfmdir)/cmbx10.tfm $(srcdir)/texb.map $@

BI: $(srcdir)/texi.map
	$(TFMTODIT) -g $(gfdir)/cmbxti10.300gf \
	  $(tfmdir)/cmbxti10.tfm $(srcdir)/texi.map $@

# CW is special because it contains "

CW: $(srcdir)/textt.map
	$(TFMTODIT) $(SPECIALFLAG) $(tfmdir)/cmtt10.tfm $(srcdir)/textt.map $@

MI: $(srcdir)/texmi.map
	$(TFMTODIT) $(SPECIALFLAG) -k 0177 $(tfmdir)/cmmi10.tfm $(srcdir)/texmi.map $@

S: $(srcdir)/texsy.map
	$(TFMTODIT) $(SPECIALFLAG) -k 060 $(tfmdir)/cmsy10.tfm $(srcdir)/texsy.map $@

EX: $(srcdir)/texex.map
	$(TFMTODIT) $(SPECIALFLAG) $(tfmdir)/cmex10.tfm $(srcdir)/texex.map $@

H: $(srcdir)/texr.map
	$(TFMTODIT) -g $(gfdir)/cmss10.300gf $(tfmdir)/cmss10.tfm $(srcdir)/texb.map $@

HB: $(srcdir)/texr.map
	$(TFMTODIT) -g $(gfdir)/cmssbx10.300gf \
	  $(tfmdir)/cmssbx10.tfm $(srcdir)/texb.map $@

HI: $(srcdir)/texr.map
	$(TFMTODIT) -g $(gfdir)/cmssi10.300gf \
	  $(tfmdir)/cmssi10.tfm $(srcdir)/texb.map $@

SA: $(srcdir)/msam.map
	$(TFMTODIT) $(SPECIALFLAG) $(tfmdir)/msam10.tfm $(srcdir)/msam.map $@

SB: $(srcdir)/msbm.map
	$(TFMTODIT) $(SPECIALFLAG) $(tfmdir)/msbm10.tfm $(srcdir)/msbm.map $@

clean:

realclean:
	-rm -f $(FONTS)

extraclean: realclean
	-rm -f core *~ \#*

.PHONY: clean realclean extraclean all
