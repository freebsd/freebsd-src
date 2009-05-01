# $FreeBSD: src/sys/modules/ida/Makefile,v 1.2.28.1 2009/04/15 03:14:26 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c \
	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
