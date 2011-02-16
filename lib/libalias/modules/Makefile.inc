# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
