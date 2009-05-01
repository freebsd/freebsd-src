# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
