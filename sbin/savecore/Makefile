#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: src/sbin/savecore/Makefile,v 1.3.8.3 2001/10/01 03:56:41 kris Exp $

PROG=	savecore
SRCS=	savecore.c
MAN=	savecore.8

DPADD+=	${LIBZ}
LDADD+=	-lz

.include <bsd.prog.mk>
