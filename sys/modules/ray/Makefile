# $FreeBSD: src/sys/modules/ray/Makefile,v 1.1.2.1 2001/01/02 23:10:22 dmlb Exp $

.PATH:	${.CURDIR}/../../dev/ray
KMOD	= if_ray
SRCS	= if_ray.c
SRCS	+= device_if.h bus_if.h card_if.h opt_inet.h
NOMAN	= 

.include <bsd.kmod.mk>
