# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
