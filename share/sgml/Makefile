#	$Id: Makefile,v 1.7 1997/01/04 22:38:26 jfieber Exp $

SUBDIR= FreeBSD ISO docbook transpec
FILES=	CATALOG
MAINTAINER=jfieber@freebsd.org

BINDIR?=/usr/share
DISTRIBUTION?=  bin

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

afterdistribute:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DISTDIR}/${DISTRIBUTION}${BINDIR}/${.CURDIR:T}

.include <bsd.subdir.mk>
