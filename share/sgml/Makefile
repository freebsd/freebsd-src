#	$Id: Makefile,v 1.4 1996/09/08 20:36:58 jfieber Exp $

SUBDIR= FreeBSD ISO transpec
FILES=	CATALOG
MAINTAINER=jfieber@freebsd.org

BINDIR?=/usr/share
DISTRIBUTION?=  doc

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

distribute:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DISTDIR}/${DISTRIBUTION}${BINDIR}/${.CURDIR:T}

.include <bsd.subdir.mk>
