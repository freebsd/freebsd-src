# $FreeBSD: src/sys/modules/uvscom/Makefile,v 1.2 2003/03/09 11:50:26 akiyama Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h vnode_if.h
NOMAN=

.include <bsd.kmod.mk>
