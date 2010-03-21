# $FreeBSD: src/sys/modules/ata/atapist/Makefile,v 1.1.24.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atapist
SRCS=	atapi-tape.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
