# $FreeBSD: src/sys/modules/vpo/Makefile,v 1.2 2000/01/28 11:26:41 bde Exp $

.PATH:	${.CURDIR}/../../dev/ppbus
KMOD=	vpo
SRCS=	bus_if.h device_if.h ppbus_if.h \
	opt_cam.h opt_scsi.h opt_vpo.h \
	immio.c vpo.c vpoio.c
NOMAN=

.include <bsd.kmod.mk>
