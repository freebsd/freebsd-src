#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/factor/Makefile,v 1.7 2003/07/24 18:30:24 markm Exp $

PROG=	factor
SRCS=	factor.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes

.if !defined(NOCRYPT) && !defined(NO_OPENSSL)
CFLAGS+=-DHAVE_OPENSSL
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

MAN=	factor.6
MLINKS+=factor.6 primes.6
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
