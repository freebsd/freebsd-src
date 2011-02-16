# $FreeBSD: src/sys/modules/ata/atacbus/Makefile,v 1.1.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atacbus
SRCS=	ata-cbus.c
SRCS+=	opt_ata.h ata_if.h device_if.h bus_if.h isa_if.h

.include <bsd.kmod.mk>
