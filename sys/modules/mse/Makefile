# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/mse

KMOD=	mse
SRCS=	mse.c device_if.h bus_if.h isa_if.h

.if (${MACHINE} == "pc98")
SRCS+= mse_cbus.c
.else
SRCS+= mse_isa.c
.endif

.include <bsd.kmod.mk>
