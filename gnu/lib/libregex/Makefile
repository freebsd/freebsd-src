# $Header: /home/ncvs/src/gnu/lib/libregex/Makefile,v 1.13 1995/03/06 05:51:32 nate Exp $


LIB=	gnuregex
CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1
SRCS=	regex.c
NOMAN=	noman

SUBDIR+= doc

beforeinstall:
	cmp -s ${.CURDIR}/regex.h ${DESTDIR}/usr/include/gnuregex.h || \
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/regex.h ${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
