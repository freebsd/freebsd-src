# $FreeBSD: src/sys/modules/umass/Makefile,v 1.7.2.2 2004/03/01 00:07:25 julian Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	umass
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_cam.h opt_scsi.h \
	umass.c

.include <bsd.kmod.mk>
