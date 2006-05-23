# $FreeBSD: src/sys/modules/esp/Makefile,v 1.3 2004/08/12 17:41:30 marius Exp $

.PATH: ${.CURDIR}/../../dev/esp

KMOD=	esp

SRCS=	ncr53c9x.c
SRCS+=	opt_ddb.h opt_cam.h
SRCS+=	device_if.h bus_if.h

.if ${MACHINE_ARCH} == "sparc64"
SRCS+=	esp_sbus.c lsi64854.c ofw_bus_if.h
.endif

.include <bsd.kmod.mk>
