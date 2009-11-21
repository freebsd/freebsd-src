# $FreeBSD: src/sys/modules/mse/Makefile,v 1.1.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mse

KMOD=	mse
SRCS=	mse.c device_if.h bus_if.h isa_if.h

.if (${MACHINE} == "pc98")
SRCS+= mse_cbus.c
.else
SRCS+= mse_isa.c
.endif

.include <bsd.kmod.mk>
