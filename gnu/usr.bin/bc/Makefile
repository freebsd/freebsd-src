# $FreeBSD: src/gnu/usr.bin/bc/Makefile,v 1.24 2004/02/17 00:56:16 ache Exp $

BCDIR=	${.CURDIR}/../../../contrib/bc
.PATH: ${BCDIR}/bc ${BCDIR}/lib ${BCDIR}/doc ${BCDIR}/Examples

PROG=	bc
SRCS=	bc.y execute.c global.c load.c main.c scan.l storage.c util.c \
	number.c
CFLAGS+=-I. -I${.CURDIR} -I${BCDIR}/h -I${BCDIR}/bc
CFLAGS+=-DHAVE_CONFIG_H
DPADD=	${LIBREADLINE} ${LIBTERMCAP}
LDADD=	-lreadline -ltermcap

FILES=	ckbook.b pi.b primes.b twins.b
FILESDIR=	${SHAREDIR}/examples/bc

.include <bsd.prog.mk>
