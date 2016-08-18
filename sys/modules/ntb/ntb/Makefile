# $FreeBSD$

.PATH:  ${.CURDIR}/../../../dev/ntb

KMOD    = ntb
SRCS    = ntb.c ntb_if.c
SRCS += device_if.h bus_if.h ntb_if.h

MFILES=	kern/bus_if.m kern/device_if.m dev/ntb/ntb_if.m

.include <bsd.kmod.mk>
