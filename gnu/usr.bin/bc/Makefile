# $FreeBSD$

BCDIR=	${.CURDIR}/../../../contrib/bc
.PATH: ${BCDIR}/bc ${BCDIR}/lib ${BCDIR}/doc ${BCDIR}/Examples

PROG=	bc
SRCS=	bc.y execute.c global.c load.c main.c scan.l storage.c util.c \
	number.c getopt.c getopt1.c
CFLAGS+=-I. -I${.CURDIR} -I${BCDIR}/h -I${BCDIR}/bc
CFLAGS+=-DHAVE_CONFIG_H
DPADD=	${LIBREADLINE} ${LIBTERMCAP}
LDADD=	-lreadline -ltermcap

FILES=	ckbook.b pi.b primes.b twins.b
FILESDIR=	${SHAREDIR}/examples/bc

.include <bsd.prog.mk>
