# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/esp

KMOD=	esp
SRCS=	device_if.h esp_pci.c ${esp_sbus} bus_if.h ncr53c9x.c ${ofw_bus_if}
SRCS+=	opt_cam.h pci_if.h

.include <bsd.kmod.mk>
