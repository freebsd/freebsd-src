# $Header: /a/cvs/386BSD/src/gnu/libregex/Makefile,v 1.4 1993/11/09 20:23:44 paul Exp $

SHLIB_MAJOR=1
SHLIB_MINOR=0

LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
