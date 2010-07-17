# $FreeBSD: src/lib/libalias/modules/Makefile.inc,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../../sys/netinet/libalias

SHLIBDIR?= /lib
LIB?=   alias_${NAME}
SHLIB_NAME?=libalias_${NAME}.so
WARNS?=	1
