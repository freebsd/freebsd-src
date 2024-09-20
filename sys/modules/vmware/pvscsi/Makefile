.PATH: ${SRCTOP}/sys/dev/vmware/pvscsi

KMOD=	pvscsi
SRCS=	pvscsi.c
SRCS+=	device_if.h bus_if.h pci_if.h
SRCS+=	opt_cam.h

.include <bsd.kmod.mk>
