#	@(#)Makefile	8.2 (Berkeley) 4/19/94

FILES=	fortunes fortunes2 startrek zippy
BLDS=	fortunes.dat fortunes2.dat startrek.dat zippy.dat \
	fortunes-o fortunes-o.dat

# TO INSTALL THE POTENTIALLY OFFENSIVE FORTUNES, UNCOMMENT THE THREE
# LINES AND COMMENT OUT THE FOURTH LINE.

# THE THREE LINES
FILES+=	fortunes2-o limerick
BLDS+=	fortunes2-o.dat limerick.dat
TYPE=	real

# THE FOURTH LINE
#TYPE=	fake

.if exists(${.OBJDIR}/../../caesar/caesar)
CAESAR=${.OBJDIR}/../../caesar/caesar
.elif exists(${.CURDIR}/../../caesar/caesar)
CAESAR=${.CURDIR}/../../caesar/caesar
.else
CAESAR=/usr/games/caesar
.endif

.if exists(${.OBJDIR}/../strfile/strfile)
STRFILE=${.OBJDIR}/../strfile/strfile
.elif exists(${.CURDIR}/../strfile/strfile)
STRFILE=${.CURDIR}/../strfile/strfile
.else
STRFILE=/usr/games/strfile
.endif

CLEANFILES+=${BLDS}

all: ${FILES} ${BLDS}

install:
	(cd ${.CURDIR} && ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${FILES} ${DESTDIR}/usr/share/games/fortune)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 ${BLDS} \
	    ${DESTDIR}/usr/share/games/fortune

fortunes.dat fortunes2.dat fortunes2-o.dat limerick.dat startrek.dat zippy.dat:
	${STRFILE} -rs ${.CURDIR}/${.TARGET:R} ${.TARGET}

fortunes-o.dat: fortunes-o
	${STRFILE} -rsx fortunes-o ${.TARGET}

fortunes-o: fortunes-o.${TYPE}
	${CAESAR} 13 < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
