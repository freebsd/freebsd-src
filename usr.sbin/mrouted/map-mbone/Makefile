# $FreeBSD: src/usr.sbin/mrouted/map-mbone/Makefile,v 1.8 1999/08/28 01:17:12 peter Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	mapper.c
MAN8=	map-mbone.8

.include <bsd.prog.mk>
