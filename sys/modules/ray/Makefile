# $FreeBSD: src/sys/modules/ray/Makefile,v 1.4.32.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h pccarddevs.h

.include <bsd.kmod.mk>
