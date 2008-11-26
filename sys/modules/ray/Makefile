# $FreeBSD: src/sys/modules/ray/Makefile,v 1.4.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h pccarddevs.h

.include <bsd.kmod.mk>
