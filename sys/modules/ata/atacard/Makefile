# $FreeBSD: src/sys/modules/ata/atacard/Makefile,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atacard
SRCS=	ata-card.c
SRCS+=  opt_ata.h pccarddevs.h ata_if.h device_if.h bus_if.h isa_if.h card_if.h

.include <bsd.kmod.mk>
