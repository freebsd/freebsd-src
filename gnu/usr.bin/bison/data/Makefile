# $Id: Makefile,v 1.7 1999/08/14 22:30:36 obrien Exp $

BISONDIR=${.CURDIR}/../../../../contrib/bison

.PATH:	${BISONDIR}

DATADIR= ${SHAREDIR}/misc
CLEANFILES+= bison.s1

all:	bison.s1

# Copy bison.simple, inserting directory name into the #line commands.
bison.s1: bison.simple
	sed -e "/^#line/ s|bison|${DATADIR}/bison|" \
	    -e "s|@bison_version@|1.28|" \
		< ${.ALLSRC} > ${.TARGET}

install:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m 444 bison.s1 \
		${DESTDIR}${DATADIR}/bison.simple
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${BISONDIR}/bison.hairy \
		${DESTDIR}${DATADIR}

.include <bsd.prog.mk>
