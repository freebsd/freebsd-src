# $Header: /home/cvs/386BSD/src/gnu/lib/libregex/Makefile,v 1.8 1994/05/27 10:53:33 csgr Exp $


LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1
SRCS=	regex.c
NOMAN=	noman

afterinstall:
	install -c ${.CURDIR}/regex.h ${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
