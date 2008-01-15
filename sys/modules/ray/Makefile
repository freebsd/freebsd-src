# $FreeBSD: src/sys/modules/ray/Makefile,v 1.4 2004/05/26 00:53:07 imp Exp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h pccarddevs.h

.include <bsd.kmod.mk>
