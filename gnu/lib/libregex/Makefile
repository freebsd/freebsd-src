# $Header: /home/cvs/386BSD/src/gnu/lib/libregex/Makefile,v 1.7 1994/05/19 17:40:01 jkh Exp $


LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman

afterinstall:
	install -c ${.CURDIR}/regex.h ${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
