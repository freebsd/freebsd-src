# $Header: /a/cvs/386BSD/src/gnu/libregex/Makefile,v 1.2 1993/08/08 04:38:56 rgrimes Exp $

LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
