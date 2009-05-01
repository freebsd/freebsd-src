#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/find/Makefile,v 1.18.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
WARNS?=6
CFLAGS+= -DHAVE_SYS_TIMEB_H -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>
