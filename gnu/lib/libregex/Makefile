# $Header: /home/ncvs/src/gnu/lib/libregex/Makefile,v 1.17 1996/08/12 19:04:23 ache Exp $


LIB=	gnuregex
CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1
SRCS=	regex.c
NOMAN=	noman

SUBDIR+= doc

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
