# $FreeBSD: src/sys/modules/umass/Makefile,v 1.12 2002/01/11 15:49:00 ru Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	umass
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_cam.h opt_scsi.h \
	umass.c

.include <bsd.kmod.mk>
