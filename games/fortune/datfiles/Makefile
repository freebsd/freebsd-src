#	@(#)Makefile	8.2 (Berkeley) 4/19/94
# $FreeBSD: src/games/fortune/datfiles/Makefile,v 1.24.2.1 2000/05/28 05:28:30 hoek Exp $

FILES=	fortunes fortunes2 murphy startrek zippy
BLDS=	fortunes.dat fortunes2.dat murphy.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat

# Pass all new entries by ${MAINTAINER} to preserve some semblance of
# humor in the fortune files.  What's funny to you on 6 beers may not
# be funny to anyone else.
MAINTAINER=	jkh

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, COMMENT OUT THE
# THREE LINES AND UNCOMMENT THE FOURTH LINE.

# THE THREE LINES:
FILES+=	fortunes2-o limerick murphy-o
BLDS+=	fortunes2-o.dat limerick.dat murphy-o.dat
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

.for f in fortunes fortunes2 fortunes2-o limerick murphy murphy-o startrek zippy
$f.dat: $f
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Crs ${.ALLSRC} ${.TARGET}
.endfor

fortunes-o.dat: fortunes-o
	PATH=$$PATH:/usr/games:${.OBJDIR}/../strfile \
	    strfile -Crsx ${.ALLSRC} ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	tr a-zA-Z n-za-mN-ZA-M < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
