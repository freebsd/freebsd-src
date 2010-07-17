#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/find/Makefile,v 1.18.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	find
SRCS=	find.c function.c ls.c main.c misc.c operator.c option.c \
	getdate.y
WARNS?=6
CFLAGS+= -DHAVE_SYS_TIMEB_H -I${.CURDIR}
YFLAGS=

.include <bsd.prog.mk>
