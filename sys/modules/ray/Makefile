# $FreeBSD: src/sys/modules/ray/Makefile,v 1.4.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ray

KMOD=	if_ray
SRCS=	if_ray.c
SRCS+=	device_if.h bus_if.h card_if.h opt_inet.h pccarddevs.h

.include <bsd.kmod.mk>
