# $FreeBSD: src/sys/modules/ubsa/Makefile,v 1.1.2.2 2003/01/14 20:04:22 kan Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ubsa
SRCS=	ubsa.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
