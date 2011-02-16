# $FreeBSD: src/usr.bin/xzdec/Makefile,v 1.2.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	xzdec

LINKS=	${BINDIR}/xzdec ${BINDIR}/lzdec

MLINKS= xzdec.1 lzmadec.1

XZDIR=	${.CURDIR}/../../contrib/xz/src
LZMALIBDIR=	${.CURDIR}/../../lib/liblzma

.PATH: ${XZDIR}/xzdec

SRCS=	xzdec.c

.PATH: ${XZDIR}/common

SRCS+=	tuklib_progname.c \
	tuklib_exit.c

WARNS?=	3

CFLAGS+=	-DHAVE_CONFIG_H \
		-I${LZMALIBDIR} \
		-I${XZDIR}/common

DPADD=	${LIBLZMA}
LDADD=	-llzma

.include <bsd.prog.mk>
