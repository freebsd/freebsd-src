# $FreeBSD$

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvisor
SRCS=	uvisor.c ucomvar.h opt_usb.h device_if.h bus_if.h vnode_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
