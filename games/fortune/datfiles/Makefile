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

# Try to find these in the build tree
.if exists(../strfile/strfile)
STRFILE=../strfile/strfile
.else
STRFILE=/usr/games/strfile
.endif

.if exists(../../caesar/caesar)
CAESAR=../../caesar/caesar
.else
CAESAR=/usr/games/caesar
.endif

CLEANFILES+=${BLDS}

all: ${FILES} ${BLDS}

install:
	(cd ${.CURDIR} && ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
		-m ${NOBINMODE} ${FILES} ${DESTDIR}${SHAREDIR}/games/fortune)
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} ${BLDS} \
	    ${DESTDIR}${SHAREDIR}/games/fortune

fortunes.dat fortunes2.dat fortunes2-o.dat limerick.dat startrek.dat zippy.dat: ${STRFILE}
	${STRFILE} -rs ${.CURDIR}/${.TARGET:R} ${.TARGET}

fortunes-o.dat: fortunes-o ${STRFILE}
	${STRFILE} -rsx fortunes-o ${.TARGET}

fortunes-o: fortunes-o.${TYPE} ${CAESAR}
	${CAESAR} 13 < ${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
