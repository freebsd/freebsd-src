# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	ata
SRCS=	ata-all.c ata-lowlevel.c ata-queue.c ata_if.c
SRCS+=	opt_ata.h ata_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
