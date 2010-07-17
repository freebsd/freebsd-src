# $FreeBSD: src/usr.bin/lzmainfo/Makefile,v 1.2.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	lzmainfo

XZDIR=	${.CURDIR}/../../contrib/xz/src
LZMALIBDIR=	${.CURDIR}/../../lib/liblzma

.PATH: ${XZDIR}/lzmainfo
SRCS+=	lzmainfo.c

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
