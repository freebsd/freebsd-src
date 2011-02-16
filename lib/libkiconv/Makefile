# $FreeBSD: src/lib/libkiconv/Makefile,v 1.6.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

LIB=		kiconv
SHLIBDIR?=	/lib
SRCS=		kiconv_sysctl.c xlat16_iconv.c xlat16_sysctl.c
SRCS+=		quirks.c

SHLIB_MAJOR=	4

MAN=		kiconv.3

MLINKS+=	kiconv.3 kiconv_add_xlat16_cspair.3 \
		kiconv.3 kiconv_add_xlat16_cspairs.3 \
		kiconv.3 kiconv_add_xlat16_table.3

CFLAGS+=	-I${.CURDIR}/../../sys

.include <bsd.lib.mk>
