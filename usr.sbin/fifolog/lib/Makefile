# $FreeBSD: src/usr.sbin/fifolog/lib/Makefile,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

LIB=	fifolog
INTERNALLIB=	# API not published or supported.

SRCS=	fifolog_int.c fifolog_create.c fifolog_write_poll.c fifolog_reader.c
SRCS+=	getdate.y

CFLAGS+= -I${.CURDIR}

.include <bsd.lib.mk>
