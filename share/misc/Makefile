#	@(#)Makefile	5.13 (Berkeley) 5/7/91
#
# PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
# --------------------         -----   ----------------------
# CURRENT PATCH LEVEL:         1       00162
# --------------------         -----   ----------------------
#
# 26 May 93	Rodney W. Grimes	Fix for codrv fonts and keycap files
#

FILES=	airport ascii birthtoken eqnchar inter.phone keycap man.template \
	mdoc.template na.phone operator zipcodes
FONTS=	8x13.bdf 8x13B.bdf 8x16.bdf 8x16elot928.bdf 8x16rk.bdf 9x15.bdf \
	9x15B.bdf clB8x10.bdf clB8x12.bdf clB8x13.bdf clB8x14.bdf clB8x16.bdf \
	clB8x8.bdf clB9x15.bdf clI8x8.bdf clR8x10.bdf clR8x12.bdf clR8x13.bdf \
	clR8x14.bdf clR8x16.bdf clR8x8.bdf clR9x15.bdf heb8x13.bdf koi8x16.bdf
NOOBJ=	noobj

all clean cleandir depend lint tags:

install:
	cd ${.CURDIR}; install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
	    ${DESTDIR}${BINDIR}/misc
	cd ${.CURDIR}/fonts; install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${FONTS} ${DESTDIR}${BINDIR}/misc/fonts

.include <bsd.prog.mk>
