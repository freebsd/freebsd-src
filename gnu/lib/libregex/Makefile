# $Header: /a/cvs/386BSD/src/gnu/libregex/Makefile,v 1.3 1993/08/23 05:26:19 rgrimes Exp $

SHLIB_MAJOR=0
SHLIB_MINOR=0

LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
