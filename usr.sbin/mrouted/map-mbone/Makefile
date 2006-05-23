# $FreeBSD: src/usr.sbin/mrouted/map-mbone/Makefile,v 1.13 2003/04/16 14:53:50 bde Exp $

S=	${.CURDIR}/..
.PATH: $S
	
PROG=	map-mbone
MAN=	map-mbone.8
SRCS=	mapper.c

CFLAGS+= -I$S

DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

.include <bsd.prog.mk>
