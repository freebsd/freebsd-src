#	$Id: Makefile,v 1.2 1996/09/08 02:43:26 jfieber Exp $

SUBDIR= FreeBSD ISO transpec
FILES=	CATALOG

BINDIR?=/usr/share

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

.include <bsd.subdir.mk>
