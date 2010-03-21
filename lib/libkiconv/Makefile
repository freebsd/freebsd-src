# $FreeBSD: src/lib/libkiconv/Makefile,v 1.4.10.1 2010/02/10 00:26:20 kensmith Exp $

LIB=		kiconv
SHLIBDIR?=	/lib
SRCS=		xlat16_iconv.c xlat16_sysctl.c
SRCS+=		quirks.c

SHLIB_MAJOR=	3

MAN=		kiconv.3

MLINKS+=	kiconv.3 kiconv_add_xlat16_cspair.3 \
		kiconv.3 kiconv_add_xlat16_cspairs.3 \
		kiconv.3 kiconv_add_xlat16_table.3

CFLAGS+=	-I${.CURDIR}/../../sys

.include <bsd.lib.mk>
