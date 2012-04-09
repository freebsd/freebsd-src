# $FreeBSD: src/sys/modules/ata/atadisk/Makefile,v 1.1.22.2.6.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../../dev/ata ${.CURDIR}/../../../${MACHINE}/${MACHINE}

KMOD=	atadisk
SRCS=	ata-disk.c ${ata_machdep}
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h

.if exists(${.CURDIR}/../../../${MACHINE}/${MACHINE}/ata_machdep.c)
ata_machdep=	ata_machdep.c
.endif

.include <bsd.kmod.mk>
