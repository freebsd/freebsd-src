# $Header: /home/ncvs/src/gnu/lib/libregex/Makefile,v 1.12 1995/01/16 17:54:57 ache Exp $


LIB=	gnuregex
CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1
SRCS=	regex.c
NOMAN=	noman

SUBDIR+= doc

beforeinstall:
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/regex.h ${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
