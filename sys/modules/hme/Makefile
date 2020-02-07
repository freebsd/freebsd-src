# $FreeBSD$

.PATH: ${SRCTOP}/sys/dev/hme

KMOD=	if_hme
SRCS=	bus_if.h device_if.h if_hme.c if_hme_pci.c miibus_if.h
SRCS+=	pci_if.h

.include <bsd.kmod.mk>
