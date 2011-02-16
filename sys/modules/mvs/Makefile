# $FreeBSD: src/sys/modules/mvs/Makefile,v 1.2.2.2.4.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/mvs

KMOD=	mvs
SRCS=	mvs.c mvs_pci.c mvs.h mvs_if.c mvs_if.h device_if.h bus_if.h pci_if.h opt_cam.h

MFILES=	kern/bus_if.m kern/device_if.m dev/pci/pci_if.m dev/mvs/mvs_if.m

.include <bsd.kmod.mk>
