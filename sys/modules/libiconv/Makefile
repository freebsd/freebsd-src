# $FreeBSD: src/sys/modules/libiconv/Makefile,v 1.1.2.2 2002/07/29 07:24:19 ru Exp $

.PATH: ${.CURDIR}/../../libkern ${.CURDIR}/../../sys

KMOD=	libiconv
SRCS=	iconv.c iconv_xlat.c
SRCS+=	iconv.h 
SRCS+=  bus_if.h device_if.h
SRCS+=	iconv_converter_if.c iconv_converter_if.h
NOMAN=	true

iconv_converter_if.h: @
	perl @/kern/makeobjops.pl -h @/libkern/iconv_converter_if.m

iconv_converter_if.c: @
	perl @/kern/makeobjops.pl -c @/libkern/iconv_converter_if.m

.include <bsd.kmod.mk>
