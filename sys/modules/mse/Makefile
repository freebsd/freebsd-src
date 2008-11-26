# $FreeBSD: src/sys/modules/mse/Makefile,v 1.1.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mse

KMOD=	mse
SRCS=	mse.c device_if.h bus_if.h isa_if.h

.if (${MACHINE} == "pc98")
SRCS+= mse_cbus.c
.else
SRCS+= mse_isa.c
.endif

.include <bsd.kmod.mk>
