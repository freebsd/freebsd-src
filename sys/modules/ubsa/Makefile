# $FreeBSD: src/sys/modules/ubsa/Makefile,v 1.1 2002/10/10 05:03:09 kan Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ubsa
SRCS=	ubsa.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
