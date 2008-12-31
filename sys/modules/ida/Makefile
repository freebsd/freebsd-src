# $FreeBSD: src/sys/modules/ida/Makefile,v 1.2.26.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c \
	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
