#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD: src/games/factor/Makefile,v 1.5 2002/10/09 19:55:04 fanf Exp $

PROG=	factor
SRCS=	factor.c pr_tbl.c
CFLAGS+=-I${.CURDIR}/../primes

.if exists(${.CURDIR}/../../crypto) && !defined(NO_OPENSSL)
CFLAGS+=-DHAVE_OPENSSL
LDADD+=	-lcrypto
DPADD+=	${LIBCRYPTO}
.endif

MAN=	factor.6
MLINKS+=factor.6 primes.6
.PATH:	${.CURDIR}/../primes

.include <bsd.prog.mk>
