# $FreeBSD: src/usr.sbin/mrouted/map-mbone/Makefile,v 1.11 2001/07/20 06:20:01 obrien Exp $

S=	${.CURDIR}/..
.PATH: $S
	
PROG=	map-mbone
MAN=	map-mbone.8
SRCS=	mapper.c

CFLAGS+= -I$S

DPADD=	${LIBMROUTED}
LDADD=	${LIBMROUTED}

.include <bsd.prog.mk>
