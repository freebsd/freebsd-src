#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/factor/Makefile,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $

.include <bsd.own.mk>

PROG=	factor
SRCS=	factor.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes

.if ${MK_OPENSSL} != "no"
CFLAGS+=-DHAVE_OPENSSL
DPADD=	${LIBCRYPTO}
LDADD=	-lcrypto
.endif

MAN=	factor.6
MLINKS+=factor.6 primes.6
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
