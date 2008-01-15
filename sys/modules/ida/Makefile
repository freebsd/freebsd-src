# $FreeBSD: src/sys/modules/ida/Makefile,v 1.2 2004/01/11 07:01:30 mdodd Exp $

.PATH:	${.CURDIR}/../../dev/ida

KMOD=	ida
SRCS=	ida.c ida_disk.c ida_pci.c \
	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
