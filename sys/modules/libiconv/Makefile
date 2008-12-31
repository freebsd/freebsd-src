# $FreeBSD: src/sys/modules/libiconv/Makefile,v 1.4.28.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../libkern ${.CURDIR}/../../sys

KMOD=	libiconv
SRCS=	iconv.c iconv_xlat.c iconv_xlat16.c
SRCS+=	iconv.h 
SRCS+=	iconv_converter_if.c iconv_converter_if.h
MFILES=	libkern/iconv_converter_if.m

EXPORT_SYMS=	iconv_open	\
		iconv_close	\
		iconv_conv	\
		iconv_conv_case	\
		iconv_convchr	\
		iconv_convchr_case	\
		iconv_convstr	\
		iconv_convmem	\
		iconv_vfs_refcount

.include <bsd.kmod.mk>
