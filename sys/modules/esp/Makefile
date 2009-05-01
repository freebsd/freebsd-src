# $FreeBSD: src/sys/modules/esp/Makefile,v 1.5.10.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/esp

KMOD=	esp
SRCS=	device_if.h ${esp_sbus} bus_if.h ncr53c9x.c ${ofw_bus_if} opt_cam.h

.if ${MACHINE} == "sparc64"
ofw_bus_if=	ofw_bus_if.h
esp_sbus=	esp_sbus.c
.endif

.include <bsd.kmod.mk>
