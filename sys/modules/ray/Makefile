# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/ray
KMOD	= if_ray
SRCS	= if_ray.c
SRCS	+= device_if.h bus_if.h card_if.h opt_inet.h
NOMAN	= 

.include <bsd.kmod.mk>
