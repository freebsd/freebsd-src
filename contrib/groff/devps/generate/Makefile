# Copyright (C) 1989-2000 Free Software Foundation, Inc.
#      Written by James Clark (jjc@jclark.com)
#
# This file is part of groff.
#
# groff is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
#
# groff is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
#
# You should have received a copy of the GNU General Public License along
# with groff; see the file COPYING.  If not, write to the Free Software
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# This is set up so you can do
#   make -f generate/Makefile
# in the parent directory of this directory.

# Directory containing AFM files.  Must not be current directory.
# Either long names (eg Times-Roman.afm) or short names (eg timesr.afm)
# may be used. See the afmname script.
afmdir=/usr/local/lib/afm
srcdir=generate

SPECIALFONTS=S
DINGBATSFONTS=ZD ZDR
GREEKFONTS=SS
TEXTFONTS=AB ABI AI AR BMB BMBI BMI BMR CB CBI CI CR HB HBI HI HR HNB HNBI \
  HNI HNR NB NBI NI NR PB PBI PI PR TB TBI TI TR ZCMI

FONTS=$(TEXTFONTS) $(SPECIALFONTS) $(GREEKFONTS) $(DINGBATSFONTS)

DESC=$(srcdir)/../DESC
AFMTODIT=afmtodit -d$(DESC)
IFLAG=-i 50
NOLIGFLAG=-n
TEXTENC=$(srcdir)/../text.enc
EFLAG=-e $(TEXTENC)
TEXTMAP=$(srcdir)/textmap
AFMNAME=$(srcdir)/afmname

all: $(FONTS)

TR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Times-Roman.afm` $(TEXTMAP) $@

TB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Times-Bold.afm` $(TEXTMAP) $@

TI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) -a 7 \
	`$(AFMNAME) $(afmdir)/Times-Italic.afm` $(TEXTMAP) $@

TBI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Times-BoldItalic.afm` $(TEXTMAP) $@

HR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica.afm` $(TEXTMAP) $@

HB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-Bold.afm` $(TEXTMAP) $@

HI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-Oblique.afm` $(TEXTMAP) $@

HBI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-BoldOblique.afm` $(TEXTMAP) $@

CR:
	$(AFMTODIT) $(NOLIGFLAG) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Courier.afm` $(TEXTMAP) $@

CB:
	$(AFMTODIT) $(NOLIGFLAG) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Courier-Bold.afm` $(TEXTMAP) $@

CI:
	$(AFMTODIT) $(NOLIGFLAG) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Courier-Oblique.afm` $(TEXTMAP) $@

CBI:
	$(AFMTODIT) $(NOLIGFLAG) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Courier-BoldOblique.afm` $(TEXTMAP) $@

PR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Palatino-Roman.afm` $(TEXTMAP) $@

PB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Palatino-Bold.afm` $(TEXTMAP) $@

PI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Palatino-Italic.afm` $(TEXTMAP) $@

PBI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Palatino-BoldItalic.afm` $(TEXTMAP) $@

NR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/NewCenturySchlbk-Roman.afm` $(TEXTMAP) $@

NB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/NewCenturySchlbk-Bold.afm` $(TEXTMAP) $@

NI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/NewCenturySchlbk-Italic.afm` $(TEXTMAP) $@

NBI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/NewCenturySchlbk-BoldItalic.afm` $(TEXTMAP) $@

BMR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Bookman-Light.afm` $(TEXTMAP) $@

BMB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Bookman-Demi.afm` $(TEXTMAP) $@

BMI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Bookman-LightItalic.afm` $(TEXTMAP) $@

BMBI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Bookman-DemiItalic.afm` $(TEXTMAP) $@

AR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/AvantGarde-Book.afm` $(TEXTMAP) $@

AB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/AvantGarde-Demi.afm` $(TEXTMAP) $@

AI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/AvantGarde-BookOblique.afm` $(TEXTMAP) $@

ABI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/AvantGarde-DemiOblique.afm` $(TEXTMAP) $@

HNR:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-Narrow.afm` $(TEXTMAP) $@

HNB:
	$(AFMTODIT) $(EFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-Narrow-Bold.afm` $(TEXTMAP) $@

HNI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-Narrow-Oblique.afm` $(TEXTMAP) $@

HNBI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/Helvetica-Narrow-BoldOblique.afm` $(TEXTMAP) $@

ZCMI:
	$(AFMTODIT) $(EFLAG) $(IFLAG) \
	`$(AFMNAME) $(afmdir)/ZapfChancery-MediumItalic.afm` $(TEXTMAP) $@

ZD:
	$(AFMTODIT) -s \
	`$(AFMNAME) $(afmdir)/ZapfDingbats.afm` $(srcdir)/dingbatsmap $@

SS: $(srcdir)/symbolsl.afm
	$(AFMTODIT) -s $(IFLAG) $(srcdir)/symbolsl.afm $(srcdir)/lgreekmap $@

S: symbol.afm
	$(AFMTODIT) -s symbol.afm symbolmap $@

ZDR: zapfdr.afm
	$(AFMTODIT) -s zapfdr.afm $(srcdir)/dingbatsrmap $@

symbol.afm: $(srcdir)/symbol.sed
	-rm -f $@
	sed -f $(srcdir)/symbol.sed `$(AFMNAME) $(afmdir)/Symbol.afm` >$@

zapfdr.afm:
	-rm -f $@
	sed -e '/^FontName /s/ZapfDingbats/ZapfDingbats-Reverse/' \
	  `$(AFMNAME) $(afmdir)/ZapfDingbats.afm` >$@

ZD: $(srcdir)/dingbatsmap
ZDR: $(srcdir)/dingbatsrmap
$(TEXTFONTS): $(TEXTMAP) $(TEXTENC)
$(SPECIALFONTS): symbolmap
$(GREEKFONTS): $(srcdir)/lgreekmap
$(FONTS): $(DESC)

symbolmap: $(TEXTMAP) $(srcdir)/symbolchars
	cat $(TEXTMAP) $(srcdir)/symbolchars >$@

clean:
	-rm -f symbolmap symbol.afm zapfdr.afm

realclean: clean
	-rm -f $(FONTS)

extraclean: realclean
	-rm -f core *~ \#*

.PHONY: all clean realclean extraclean

