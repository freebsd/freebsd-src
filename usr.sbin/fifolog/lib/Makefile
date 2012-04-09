# $FreeBSD: src/usr.sbin/fifolog/lib/Makefile,v 1.2.2.1.8.1 2012/03/03 06:15:13 kensmith Exp $

LIB=	fifolog
INTERNALLIB=	# API not published or supported.

SRCS=	fifolog_int.c fifolog_create.c fifolog_write_poll.c fifolog_reader.c
SRCS+=	getdate.y

CFLAGS+= -I${.CURDIR}

.include <bsd.lib.mk>
