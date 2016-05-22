# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/vmware/vmw_pvscsi

KMOD=	vmw_pvscsi
SRCS=	vmw_pvscsi.c

CLEANFILES+=	bus_if.h device_if.h opt_cam.h pci_if.h
SRCS+=		bus_if.h device_if.h opt_cam.h pci_if.h

.include <bsd.kmod.mk>
