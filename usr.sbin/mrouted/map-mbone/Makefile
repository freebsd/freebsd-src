# $FreeBSD: src/usr.sbin/mrouted/map-mbone/Makefile,v 1.13.24.1 2008/10/02 02:57:24 kensmith Exp $

S=	${.CURDIR}/..
.PATH: $S
	
PROG=	map-mbone
MAN=	map-mbone.8
SRCS=	mapper.c

CFLAGS+= -I$S

DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

.include <bsd.prog.mk>
