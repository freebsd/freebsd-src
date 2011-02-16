# $FreeBSD: src/sys/modules/mse/Makefile,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mse

KMOD=	mse
SRCS=	mse.c device_if.h bus_if.h isa_if.h

.if (${MACHINE} == "pc98")
SRCS+= mse_cbus.c
.else
SRCS+= mse_isa.c
.endif

.include <bsd.kmod.mk>
