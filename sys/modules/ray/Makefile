# $FreeBSD: src/sys/modules/ray/Makefile,v 1.3 2002/01/11 15:48:57 ru Exp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h

.include <bsd.kmod.mk>
