#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/find/Makefile,v 1.18.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
WARNS?=6
CFLAGS+= -DHAVE_SYS_TIMEB_H -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>
