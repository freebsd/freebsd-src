#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	factor
SRCS=	factor.c pr_tbl.c
CFLAGS+=-I${SRCTOP}/usr.bin/primes

.if ${MK_OPENSSL} != "no"
CFLAGS+=-DHAVE_OPENSSL
CFLAGS+=-DOPENSSL_API_COMPAT=0x10100000L
LIBADD+=crypto
.endif

MAN=	factor.6
MLINKS+=factor.6 primes.6
.PATH:	${SRCTOP}/usr.bin/primes

.include <bsd.prog.mk>
