# $FreeBSD: src/sys/modules/uftdi/Makefile,v 1.1.2.2 2003/07/21 11:39:21 akiyama Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uftdi
SRCS=	uftdi.c uftdireg.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
