# $Id: Makefile,v 1.1 1996/09/10 13:23:44 peter Exp $

BISONDIR=${.CURDIR}/../../../../contrib/bison

.PATH:	${BISONDIR}

DATADIR= ${SHAREDIR}/misc
CLEANFILES+= bison.s1

all:	bison.s1

# Copy bison.simple, inserting directory name into the #line commands.
bison.s1: bison.simple
	sed -e "/^#line/ s|bison|${DATADIR}/bison|" \
		< ${BISONDIR}/bison.simple > bison.s1

install:
	${INSTALL} -c -o bin -g bin -m 444 bison.s1 \
		${DESTDIR}${DATADIR}/bison.simple
	${INSTALL} -c -o bin -g bin -m 444 ${BISONDIR}/bison.hairy \
		${DESTDIR}${DATADIR}

.include <bsd.prog.mk>
