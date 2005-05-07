#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/factor/Makefile,v 1.8 2004/02/05 22:44:20 ru Exp $

PROG=	factor
SRCS=	factor.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
CFLAGS+=-DHAVE_OPENSSL
LDADD=	-lcrypto
DPADD=	${LIBCRYPTO}
.endif

MAN=	factor.6
MLINKS+=factor.6 primes.6
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
