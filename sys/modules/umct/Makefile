# $FreeBSD: src/sys/modules/umct/Makefile,v 1.2.2.1 2004/04/13 03:39:17 rwatson Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	umct
SRCS=	umct.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
