# $FreeBSD$

.PATH: ${.CURDIR}/../../libkern ${.CURDIR}/../../sys

KMOD=	libiconv
SRCS=	iconv.c iconv_xlat.c
SRCS+=	iconv.h 
SRCS+=	iconv_converter_if.c iconv_converter_if.h
MFILES=	libkern/iconv_converter_if.m

EXPORT_SYMS=	iconv_open	\
		iconv_close	\
		iconv_conv	\
		iconv_convstr	\
		iconv_convmem

.include <bsd.kmod.mk>
