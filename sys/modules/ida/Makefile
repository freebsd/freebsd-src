# $FreeBSD: src/sys/modules/ida/Makefile,v 1.2.34.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c \
	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
