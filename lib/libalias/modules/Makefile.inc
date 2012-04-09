# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3.10.1.8.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
