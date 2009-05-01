# $FreeBSD: src/usr.sbin/fifolog/lib/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

LIB =		fifolog
INTERNALLIB =	API not published or supported.

SRCS +=		fifolog_int.c
SRCS +=		fifolog_create.c
SRCS +=		fifolog_write_poll.c
SRCS +=		fifolog_reader.c
SRCS +=		getdate.y

CFLAGS +=	-I${.CURDIR}

.include <bsd.lib.mk>

test:
	echo ${HAS_EXPAND_NUMBER}
