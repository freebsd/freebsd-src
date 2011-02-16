# $FreeBSD: src/sys/modules/ata/atacam/Makefile,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atacam
SRCS=	ata-cam.c
SRCS+=  opt_ata.h opt_cam.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
