# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/bfe

KMOD=	if_bfe
SRCS=	if_bfe.c miibus_if.h opt_bdg.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
