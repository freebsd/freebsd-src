# Copyright (C) 1994, 2002 Free Software Foundation, Inc.
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
# Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# Directory containing Autofont TFM files.
# The TFM files I used are available for ftp as
#
#   ftp://ftp.hp.com/pub/faxes/software/tfmlj4p.exe
#
# This is a self-extracting MS-DOS archive (in ZIP format).

# This is set up so you can do
#   make -f generate/Makefile
# in the parent directory of this directory.

AUTOFONT=/usr/local/lib/groff-lj4
HPFTODIT=hpftodit
# .05 em
IFLAG=-i 439
SFLAG=-s
srcdir=generate
TEXTMAP=$(srcdir)/text.map
SPECIALMAP=$(srcdir)/special.map

FONTS=\
  ALBB ALBR \
  AOB AOI AOR \
  CB CBI CI CR \
  GB GBI GI GR \
  LGB LGI LGR \
  OB OBI OI OR \
  TB TBI TI TR \
  UB UBI UI UR \
  UCB UCBI UCI UCR \
  CLARENDON CORONET MARIGOLD S

all: $(FONTS)

TR: $(AUTOFONT)/9nb00086.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00086.tfm $(TEXTMAP) $@

TI: $(AUTOFONT)/9nb00084.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00084.tfm $(TEXTMAP) $@

TB: $(AUTOFONT)/9nb00083.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00083.tfm $(TEXTMAP) $@

TBI: $(AUTOFONT)/9nb00085.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00085.tfm $(TEXTMAP) $@

CR: $(AUTOFONT)/9nb00064.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00064.tfm $(TEXTMAP) $@

CI: $(AUTOFONT)/9nb00062.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00062.tfm $(TEXTMAP) $@

CB: $(AUTOFONT)/9nb00061.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00061.tfm $(TEXTMAP) $@

CBI: $(AUTOFONT)/9nb00063.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00063.tfm $(TEXTMAP) $@

GR: $(AUTOFONT)/9nb00068.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00068.tfm $(TEXTMAP) $@

GI: $(AUTOFONT)/9nb00066.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00066.tfm $(TEXTMAP) $@

GB: $(AUTOFONT)/9nb00065.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00065.tfm $(TEXTMAP) $@

GBI: $(AUTOFONT)/9nb00067.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00067.tfm $(TEXTMAP) $@

OR: $(AUTOFONT)/9nb00076.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00076.tfm $(TEXTMAP) $@

OI: $(AUTOFONT)/9nb00074.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00074.tfm $(TEXTMAP) $@

OB: $(AUTOFONT)/9nb00073.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00073.tfm $(TEXTMAP) $@

OBI: $(AUTOFONT)/9nb00075.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00075.tfm $(TEXTMAP) $@

UR: $(AUTOFONT)/9nb00094.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00094.tfm $(TEXTMAP) $@

UI: $(AUTOFONT)/9nb00092.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00092.tfm $(TEXTMAP) $@

UB: $(AUTOFONT)/9nb00088.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00088.tfm $(TEXTMAP) $@

UBI: $(AUTOFONT)/9nb00093.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00093.tfm $(TEXTMAP) $@

UCR: $(AUTOFONT)/9nb00089.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00089.tfm $(TEXTMAP) $@

UCI: $(AUTOFONT)/9nb00087.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00087.tfm $(TEXTMAP) $@

UCB: $(AUTOFONT)/9nb00090.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00090.tfm $(TEXTMAP) $@

UCBI: $(AUTOFONT)/9nb00091.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00091.tfm $(TEXTMAP) $@

ALBR: $(AUTOFONT)/9nb00052.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00052.tfm $(TEXTMAP) $@

ALBB: $(AUTOFONT)/9nb00051.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00051.tfm $(TEXTMAP) $@

LGR: $(AUTOFONT)/9nb00071.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00071.tfm $(TEXTMAP) $@

LGB: $(AUTOFONT)/9nb00069.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00069.tfm $(TEXTMAP) $@

LGI: $(AUTOFONT)/9nb00070.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00070.tfm $(TEXTMAP) $@

AOR: $(AUTOFONT)/9nb00055.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00055.tfm $(TEXTMAP) $@

AOI: $(AUTOFONT)/9nb00054.tfm $(TEXTMAP)
	$(HPFTODIT) $(IFLAG) $(AUTOFONT)/9nb00054.tfm $(TEXTMAP) $@

AOB: $(AUTOFONT)/9nb00053.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00053.tfm $(TEXTMAP) $@

MARIGOLD: $(AUTOFONT)/9nb00072.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00072.tfm $(TEXTMAP) $@

CORONET: $(AUTOFONT)/9nb00070.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00070.tfm $(TEXTMAP) $@

CLARENDON: $(AUTOFONT)/9nb00060.tfm $(TEXTMAP)
	$(HPFTODIT) $(AUTOFONT)/9nb00060.tfm $(TEXTMAP) $@

S: $(AUTOFONT)/9nb00086.tfm $(SPECIALMAP)
	$(HPFTODIT) $(SFLAG) $(AUTOFONT)/9nb00086.tfm $(SPECIALMAP) $@

clean:

realclean:
	-rm -f $(FONTS)

extraclean: realclean
	-rm -f core *~ "#*"

.PHONY: clean realclean extraclean all
