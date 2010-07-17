#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/cp/Makefile,v 1.12.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	cp
SRCS=	cp.c utils.c
CFLAGS+= -DVM_AND_BUFFER_CACHE_SYNCHRONIZED -D_ACL_PRIVATE

.include <bsd.prog.mk>
