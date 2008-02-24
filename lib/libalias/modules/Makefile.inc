# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3 2006/10/08 14:01:59 piso Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
