# $FreeBSD: src/gnu/lib/libregex/Makefile,v 1.22 1999/08/28 04:58:19 peter Exp $

SUBDIR+= doc

LIB=		gnuregex
SHLIB_MAJOR=	2
SHLIB_MINOR=	0

SRCS=	regex.c
NOMAN=	noman

CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1 -DNO_POSIX_COMPAT=1

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
