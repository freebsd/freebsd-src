#	@(#)Makefile	8.2 (Berkeley) 4/19/94

SRCS=	fortunes fortunes2 startrek zippy
BLDS=	fortunes.dat fortunes2.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat

# TO INSTALL THE POTENTIALLY OFFENSIVE FORTUNES, UNCOMMENT THE THREE
# LINES AND COMMENT OUT THE FOURTH LINE.

# THE THREE LINES
SRCS+=	fortunes2-o limerick
BLDS+=	fortunes2-o.dat limerick.dat
TYPE=	real

# THE FOURTH LINE
#TYPE=	fake

CLEANFILES+=${BLDS}

install: ${SRCS} ${BLDS}
	(cd ${.CURDIR} && install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${SRCS} ${DESTDIR}/usr/share/games/fortune)
	install -o ${BINOWN} -g ${BINGRP} -m 444 ${BLDS} \
	    ${DESTDIR}/usr/share/games/fortune

fortunes.dat fortunes2.dat fortunes2-o.dat limerick.dat startrek.dat zippy.dat:
	${.CURDIR}/../strfile/obj/strfile -rs \
	    ${.CURDIR}/${.TARGET:R} ${.TARGET}

fortunes-o.dat: fortunes-o
	${.CURDIR}/../strfile/obj/strfile -rsx fortunes-o ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	PATH=../../caesar:$$PATH ; caesar 13 < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
