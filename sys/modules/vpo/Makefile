# $FreeBSD: src/sys/modules/vpo/Makefile,v 1.6.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ppbus

KMOD=	vpo
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_cam.h opt_scsi.h opt_vpo.h \
	immio.c vpo.c vpoio.c

.include <bsd.kmod.mk>
