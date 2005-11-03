#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/pac/Makefile,v 1.11 2002/06/23 20:57:18 gad Exp $

.PATH: ${.CURDIR}/../common_source

PROG=	pac
MAN=	pac.8

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
