# $FreeBSD$

LIB=		mp
CFLAGS+=	-ansi -pedantic
WARNS?=		2
NO_WERROR=	yes
SHLIB_MAJOR=	4
SRCS=		mpasbn.c
INCS=		mp.h
MAN3=		mp.3
MLINKS=		mp.3 libmp.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/mp.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
