# $FreeBSD$

.PATH: ${.CURDIR}/../../libkern ${.CURDIR}/../../sys

KMOD=	libiconv
SRCS=	iconv.c iconv_xlat.c
SRCS+=	iconv.h 
SRCS+=	iconv_converter_if.c iconv_converter_if.h
MFILES=	libkern/iconv_converter_if.m
NOMAN=	true

.include <bsd.kmod.mk>
