#	$Id: Makefile,v 1.3 1996/09/08 02:58:28 jfieber Exp $

SUBDIR= FreeBSD ISO transpec
FILES=	CATALOG
MAINTAINER=jfieber@freebsd.org

BINDIR?=/usr/share

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.subdir.mk>
