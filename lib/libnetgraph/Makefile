# $FreeBSD$
# $Whistle: Makefile,v 1.4 1999/01/17 03:41:02 julian Exp $

LIB=		netgraph
MAN3=		netgraph.3

SHLIB_MAJOR=	1
#SHLIB_MINOR=	1

SRCS=		sock.c msg.c debug.c

CFLAGS+=	-g -Wall -O2 -Werror
CFLAGS+=	-I/usr/src.freefall/sys

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/netgraph.h \
		${DESTDIR}/usr/include

.include <bsd.lib.mk>
