#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/bin/cp/Makefile,v 1.12.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	cp
SRCS=	cp.c utils.c
CFLAGS+= -DVM_AND_BUFFER_CACHE_SYNCHRONIZED -D_ACL_PRIVATE

.include <bsd.prog.mk>
