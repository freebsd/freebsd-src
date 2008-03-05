# $FreeBSD$

S=	${.CURDIR}/../..
.PATH: $S/dev/usb

KMOD=	uslcom
SRCS=	uslcom.c opt_usb.h opt_uslcom.h \
	device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
