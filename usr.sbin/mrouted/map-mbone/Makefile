# $FreeBSD: src/usr.sbin/mrouted/map-mbone/Makefile,v 1.7.2.1 1999/08/29 15:44:06 peter Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	mapper.c
MAN8=	map-mbone.8

.include <bsd.prog.mk>
