# $Header: /home/ncvs/src/gnu/lib/libregex/Makefile,v 1.9 1994/09/15 19:52:50 bde Exp $


LIB=	gnuregex
CFLAGS+=-DHAVE_STRING_H=1 -DSTDC_HEADERS=1
SRCS=	regex.c
NOMAN=	noman

afterinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/regex.h ${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
