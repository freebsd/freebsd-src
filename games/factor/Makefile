#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

.include <src.opts.mk>

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
