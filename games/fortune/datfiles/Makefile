#	@(#)Makefile	8.2 (Berkeley) 4/19/94
# $FreeBSD$

FILES=	fortunes fortunes2 startrek zippy
BLDS=	fortunes.dat fortunes2.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, COMMENT OUT THE
# THREE LINES AND UNCOMMENT THE FOURTH LINE.

# THE THREE LINES:
FILES+=	fortunes2-o limerick
BLDS+=	fortunes2-o.dat limerick.dat
TYPE=	real

# THE FOURTH LINE:
#TYPE=	fake

CLEANFILES+=${BLDS}

all: ${BLDS}

beforeinstall:
	(cd ${.CURDIR} && ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
	    -m ${NOBINMODE} ${FILES} ${DESTDIR}${SHAREDIR}/games/fortune)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} ${BLDS} \
	    ${DESTDIR}${SHAREDIR}/games/fortune

.for f in fortunes fortunes2 fortunes2-o limerick startrek zippy
$f.dat: $f
	PATH={.OBJDIR}/../strfile:$$PATH:/usr/games \
	    strfile -Crs ${.ALLSRC} ${.TARGET}
.endfor

fortunes-o.dat: fortunes-o
	PATH=${.OBJDIR}/../strfile:$$PATH:/usr/games \
	    strfile -Crsx ${.ALLSRC} ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	PATH=${.OBJDIR}/../../caesar:$$PATH:/usr/games \
	    caesar 13 < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
