# $Header: /home/ncvs/src/gnu/lib/libregex/Makefile,v 1.19 1996/11/16 22:53:42 joerg Exp $


LIB=	gnuregex
CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1 -DNO_POSIX_COMPAT=1
SRCS=	regex.c
NOMAN=	noman

SUBDIR+= doc

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/regex.h \
		${DESTDIR}/usr/include/gnuregex.h

.include <bsd.lib.mk>
