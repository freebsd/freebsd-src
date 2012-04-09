# $FreeBSD: src/sys/modules/cryptodev/Makefile,v 1.2.10.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	${.CURDIR}/../../opencrypto
KMOD	= cryptodev
SRCS	= cryptodev.c
SRCS	+= bus_if.h device_if.h opt_compat.h

.include <bsd.kmod.mk>
