# $FreeBSD: src/usr.sbin/mrouted/map-mbone/Makefile,v 1.8.2.1 2001/04/25 12:10:09 ru Exp $

PROG=	map-mbone

S=	${.CURDIR}/..
.PATH:	$S
CFLAGS+=	-I$S
DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

SRCS=	mapper.c
MAN=	map-mbone.8

.include <bsd.prog.mk>
