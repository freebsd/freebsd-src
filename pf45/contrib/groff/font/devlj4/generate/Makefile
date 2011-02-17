# Copyright (C) 1994, 2002, 2003, 2004 Free Software Foundation, Inc.
#     Written by James Clark (jjc@jclark.com)
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
# Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA.

# Directory containing Autofont TFM files.
# The TFM files used are available for ftp as
#
#   ftp://ftp.hp.com/pub/faxes/software/tfmlj4p.exe
#
# This is a self-extracting MS-DOS archive (in ZIP format).
#
# Metric files for the Arial and Times New Roman families and the Symbol
# font have been replaced with versions which have more glyphs and/or
# kerning pairs:
#
#   ftp://ftp.ffii.org/pub/groff/contrib/lj4/tfmlj4x.zip

# This is set up so you can do
#   make -f generate/Makefile
# in the parent directory of this directory.

AUTOFONT=/usr/local/lib/groff-lj4
HPFTODIT=hpftodit
# .05 em
IFLAG=-i 50
SFLAG=-s
srcdir=generate
TEXTMAP=$(srcdir)/text.map
SPECIALMAP=$(srcdir)/special.map
SYMBOLMAP=$(srcdir)/symbol.map
WINGDINGSMAP=$(srcdir)/wingdings.map

FONTS=\
  AB ABI AI AR \
  ALBB ALBR \
  AOB AOI AOR \
  CB CBI CI CR \
  GB GBI GI GR \
  LGB LGI LGR \
  OB OBI OI OR \
  TB TBI TI TR \
  TNRB TNRBI TNRI TNRR \
  UB UBI UI UR \
  UCB UCBI UCI UCR \
  CLARENDON CORONET MARIGOLD S \
  SYMBOL WINGDINGS

all: $(FONTS)

#
# Intellifont
#

# CG Times
TR: $(AUTOFONT)/9nb00086.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00086.tfm $(TEXTMAP) $@

# CG Times Italic
TI: $(AUTOFONT)/9nb00084.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00084.tfm $(TEXTMAP) $@

# CG Times Bold
TB: $(AUTOFONT)/9nb00083.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00083.tfm $(TEXTMAP) $@

# CG Times Bold Italic
TBI: $(AUTOFONT)/9nb00085.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00085.tfm $(TEXTMAP) $@

# Courier
CR: $(AUTOFONT)/9nb00064.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00064.tfm $(TEXTMAP) $@

# Courier Italic
CI: $(AUTOFONT)/9nb00062.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00062.tfm $(TEXTMAP) $@

# Courier Bold
CB: $(AUTOFONT)/9nb00061.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00061.tfm $(TEXTMAP) $@

# Courier Bold Italic
CBI: $(AUTOFONT)/9nb00063.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00063.tfm $(TEXTMAP) $@

# Garamond Antiqua
GR: $(AUTOFONT)/9nb00068.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00068.tfm $(TEXTMAP) $@

# Garamond Kursiv
GI: $(AUTOFONT)/9nb00066.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00066.tfm $(TEXTMAP) $@

# Garamond Halbfett
GB: $(AUTOFONT)/9nb00065.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00065.tfm $(TEXTMAP) $@

# Garamond Kursiv Halbfett
GBI: $(AUTOFONT)/9nb00067.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00067.tfm $(TEXTMAP) $@

# Omega
OR: $(AUTOFONT)/9nb00076.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00076.tfm $(TEXTMAP) $@

# Omega Italic
OI: $(AUTOFONT)/9nb00074.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00074.tfm $(TEXTMAP) $@

# Omega Bold
OB: $(AUTOFONT)/9nb00073.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00073.tfm $(TEXTMAP) $@

# Omega Bold Italic
OBI: $(AUTOFONT)/9nb00075.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00075.tfm $(TEXTMAP) $@

# Univers
UR: $(AUTOFONT)/9nb00094.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00094.tfm $(TEXTMAP) $@

# Univers Italic
UI: $(AUTOFONT)/9nb00092.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00092.tfm $(TEXTMAP) $@

# Univers Bold
UB: $(AUTOFONT)/9nb00088.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00088.tfm $(TEXTMAP) $@

# Univers Bold Italic
UBI: $(AUTOFONT)/9nb00093.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00093.tfm $(TEXTMAP) $@

# Univers Condensed
UCR: $(AUTOFONT)/9nb00089.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00089.tfm $(TEXTMAP) $@

# Univers Condensed Italic
UCI: $(AUTOFONT)/9nb00087.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00087.tfm $(TEXTMAP) $@

# Univers Condensed Bold
UCB: $(AUTOFONT)/9nb00090.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00090.tfm $(TEXTMAP) $@

# Univers Condensed Bold Italic
UCBI: $(AUTOFONT)/9nb00091.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00091.tfm $(TEXTMAP) $@

# Albertus
ALBR: $(AUTOFONT)/9nb00052.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00052.tfm $(TEXTMAP) $@

# Albertus Extrabold
ALBB: $(AUTOFONT)/9nb00051.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00051.tfm $(TEXTMAP) $@

# Letter Gothic
LGR: $(AUTOFONT)/9nb00071.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00071.tfm $(TEXTMAP) $@

# Letter Gothic Italic
LGI: $(AUTOFONT)/9nb00070.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00070.tfm $(TEXTMAP) $@

# Letter Gothic Bold
LGB: $(AUTOFONT)/9nb00069.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00069.tfm $(TEXTMAP) $@

# Antique Olive
AOR: $(AUTOFONT)/9nb00055.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00055.tfm $(TEXTMAP) $@

# Antique Olive Italic
AOI: $(AUTOFONT)/9nb00054.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00054.tfm $(TEXTMAP) $@

# Antique Olive Bold
AOB: $(AUTOFONT)/9nb00053.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00053.tfm $(TEXTMAP) $@

# Marigold
MARIGOLD: $(AUTOFONT)/9nb00072.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00072.tfm $(TEXTMAP) $@

# Coronet
CORONET: $(AUTOFONT)/9nb00077.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00077.tfm $(TEXTMAP) $@

# Clarendon Condensed
CLARENDON: $(AUTOFONT)/9nb00060.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00060.tfm $(TEXTMAP) $@

S: $(AUTOFONT)/9nb00086.tfm $(SPECIALMAP) $(srcdir)/special.awk
	$(HPFTODIT) $(SFLAG) $(AUTOFONT)/9nb00086.tfm $(SPECIALMAP) - \
	| awk -f $(srcdir)/special.awk > $@

#
# TrueType
#

# Times New Roman
TNRR: $(AUTOFONT)/tmnr000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/tmnr000s.tfm $(TEXTMAP) $@

# Times New Roman Italic
TNRI: $(AUTOFONT)/tmni000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/tmni000s.tfm $(TEXTMAP) $@

# Times New Roman Bold
TNRB: $(AUTOFONT)/tmnb000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/tmnb000s.tfm $(TEXTMAP) $@

# Times New Roman Bold Italic
TNRBI: $(AUTOFONT)/tmnj000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/tmnj000s.tfm $(TEXTMAP) $@

# Arial
AR: $(AUTOFONT)/arlr000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/arlr000s.tfm $(TEXTMAP) $@

# Arial Italic
AI: $(AUTOFONT)/arli000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/arli000s.tfm $(TEXTMAP) $@

# Arial Bold
AB: $(AUTOFONT)/arlb000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/arlb000s.tfm $(TEXTMAP) $@

# Arial Bold Italic
ABI: $(AUTOFONT)/arlj000s.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/arlj000s.tfm $(TEXTMAP) $@

# Symbol
SYMBOL: $(AUTOFONT)/9nb28703.tfm $(SYMBOLMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb28703.tfm $(SYMBOLMAP) $@

# Wingdings
WINGDINGS: $(AUTOFONT)/9nb00095.tfm $(WINGDINGSMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00095.tfm $(WINGDINGSMAP) $@


clean:

realclean:
	-rm -f $(FONTS)

extraclean: realclean
	-rm -f core *~ "#*"

.PHONY: clean realclean extraclean all
