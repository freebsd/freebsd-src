# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/lge

KMOD=	if_lge
SRCS=	if_lge.c miibus_if.h opt_bdg.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
