# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/ata

KMOD=	atadevel
SRCS=	ata-devel.c
SRCS+=  opt_ata.h ata_if.h device_if.h bus_if.h pci_if.h
CFLAGS+=-g

.include <bsd.kmod.mk>
