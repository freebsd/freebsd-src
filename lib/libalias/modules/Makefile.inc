# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
