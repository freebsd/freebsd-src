# $FreeBSD: src/sys/modules/uvscom/Makefile,v 1.1.2.3 2003/07/21 12:19:23 akiyama Exp $

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h vnode_if.h
NOMAN=
KMODDEPS= ucom

.include <bsd.kmod.mk>
