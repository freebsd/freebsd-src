# $FreeBSD: src/sys/modules/esp/Makefile,v 1.6.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/esp

KMOD=	esp
SRCS=	device_if.h esp_pci.c ${esp_sbus} bus_if.h ncr53c9x.c ${ofw_bus_if}
SRCS+=	opt_cam.h pci_if.h

.if ${MACHINE} == "sparc64"
ofw_bus_if=	ofw_bus_if.h
esp_sbus=	esp_sbus.c
.endif

.include <bsd.kmod.mk>
