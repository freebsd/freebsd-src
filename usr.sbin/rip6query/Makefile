#	$NetBSD: Makefile,v 1.7 1995/03/18 14:56:21 cgd Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD$

PROG=	rip6query
WARNS?=	2
MAN=	rip6query.8

CFLAGS+=-DINET6 -I${.CURDIR}/../route6d

.include <bsd.prog.mk>
