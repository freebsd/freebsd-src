#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/xargs/Makefile,v 1.1.1.1.14.1 2002/06/17 04:44:46 jmallett Exp $

PROG=	xargs
SRCS=	xargs.c strnsubst.c
WARNS?=	4

.if defined(BOOTSTRAPPING)
CFLAGS+=-DBOOTSTRAPPING
.endif

.include <bsd.prog.mk>
