# $FreeBSD: src/sys/modules/vpo/Makefile,v 1.6.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ppbus

KMOD=	vpo
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_cam.h opt_scsi.h opt_vpo.h \
	immio.c vpo.c vpoio.c

.include <bsd.kmod.mk>
