# $FreeBSD: src/sys/modules/ray/Makefile,v 1.4.28.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h pccarddevs.h

.include <bsd.kmod.mk>
