#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD$

PROG=	savecore
SRCS=	savecore.c
WARNS=	0
MAN=	savecore.8

DPADD+=	${LIBZ}
LDADD+=	-lz

.include <bsd.prog.mk>
