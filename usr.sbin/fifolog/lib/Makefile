# $FreeBSD: src/usr.sbin/fifolog/lib/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIB=	fifolog
INTERNALLIB=	# API not published or supported.

SRCS=	fifolog_int.c fifolog_create.c fifolog_write_poll.c fifolog_reader.c
SRCS+=	getdate.y

CFLAGS+= -I${.CURDIR}

.include <bsd.lib.mk>
