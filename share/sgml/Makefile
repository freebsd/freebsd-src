#	$FreeBSD$

SUBDIR= FreeBSD ISO docbook transpec
FILES=	CATALOG
MAINTAINER=jfieber@freebsd.org

BINDIR?=/usr/share
DISTRIBUTION?=  doc

afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DESTDIR}${BINDIR}/${.CURDIR:T}

afterdistribute:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${FILES} \
		${DISTDIR}/${DISTRIBUTION}${BINDIR}/${.CURDIR:T}

.include <bsd.subdir.mk>
