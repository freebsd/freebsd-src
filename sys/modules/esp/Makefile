# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/esp

KMOD=	esp
SRCS=	device_if.h esp_pci.c bus_if.h ncr53c9x.c
SRCS+=	opt_cam.h pci_if.h

.include <bsd.kmod.mk>
