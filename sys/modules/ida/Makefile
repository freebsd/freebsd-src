# $FreeBSD: src/sys/modules/ida/Makefile,v 1.2.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c \
	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
