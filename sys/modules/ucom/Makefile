# $FreeBSD: src/sys/modules/ucom/Makefile,v 1.1.2.1 2002/08/08 18:45:05 joe Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	ucom
SRCS=	ucom.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
