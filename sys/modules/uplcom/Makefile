# $FreeBSD: src/sys/modules/uplcom/Makefile,v 1.2 2003/11/16 12:26:10 akiyama Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uplcom
SRCS=	uplcom.c ucomvar.h opt_usb.h opt_uplcom.h \
	device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
