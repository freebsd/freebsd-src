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
#Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

FFLAG=-F..
TROFF=../troff/troff -M../tmac $(FFLAG)
GROPS=../grops/grops $(FFLAG)
DOCS=meref.PS meintro.PS
MEMACROS=../macros/tmac.e
SOELIM=../soelim/soelim

all: $(DOCS)

.SUFFIXES: .tr .me .PS .dit

.dit.PS:
	$(GROPS) $< >$@

.me.dit:
	$(SOELIM) $< \
	| sed -e "s;@VERSION@;`cat ../VERSION`;" \
	| $(TROFF) -Tps $(FFLAG) -me  >$@

.tr.dit:
	$(TROFF) -Tps $< >$@

meref.PS: meref.dit
meintro.PS: meintro.dit

install:
	
clean:
	-rm -f *.PS *.dit core

distclean: clean

realclean: distclean

extraclean: clean
	-rm -f core *~ \#* junk temp grot
