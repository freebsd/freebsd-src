# $FreeBSD$

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uvscom
SRCS=	uvscom.c ucomvar.h opt_usb.h opt_uvscom.h \
	device_if.h bus_if.h vnode_if.h usbdevs.h

.include <bsd.kmod.mk>
