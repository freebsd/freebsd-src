# $FreeBSD: src/sys/modules/ata/atadevel/Makefile,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atadevel
SRCS=	ata-devel.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h
CFLAGS+=-g

.include <bsd.kmod.mk>
