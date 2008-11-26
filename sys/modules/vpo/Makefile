# $FreeBSD: src/sys/modules/vpo/Makefile,v 1.6.30.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ppbus

KMOD=	vpo
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_cam.h opt_scsi.h opt_vpo.h \
	immio.c vpo.c vpoio.c

.include <bsd.kmod.mk>
