# $FreeBSD: src/sys/modules/ata/atadevel/Makefile,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atadevel
SRCS=	ata-devel.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h
CFLAGS+=-g

.include <bsd.kmod.mk>
