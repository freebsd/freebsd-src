# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.1.2.3 2003/11/30 13:05:38 akiyama Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h vnode_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
