# $FreeBSD: src/sys/modules/ata/atacard/Makefile,v 1.1.26.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atacard
SRCS=	ata-card.c
SRCS+=  opt_ata.h pccarddevs.h ata_if.h device_if.h bus_if.h isa_if.h card_if.h

.include <bsd.kmod.mk>
