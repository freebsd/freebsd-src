# $FreeBSD$

LIB=		mp
SHLIB_MAJOR=	4
CFLAGS+=	-ansi -pedantic
WARNS?=		2
# openssl/bn.h isn't WARNS-safe, but I still want the warings.
NO_WERROR=	yes
SRCS=		mpasbn.c
INCS=		mp.h
MAN3=		mp.3
MLINKS=		mp.3 libmp.3

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/mp.h ${DESTDIR}/usr/include

.include <bsd.lib.mk>
