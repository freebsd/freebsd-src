# $FreeBSD: src/usr.sbin/fifolog/lib/Makefile,v 1.1.2.1.6.1 2010/02/10 00:26:20 kensmith Exp $

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
