# $FreeBSD: src/sys/modules/uvisor/Makefile,v 1.1 2002/07/30 17:44:28 joe Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvisor
SRCS=	uvisor.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
