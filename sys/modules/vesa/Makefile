# $FreeBSD: src/sys/modules/vesa/Makefile,v 1.14.36.2.2.1 2010/06/14 02:09:06 kensmith Exp $

KMOD=	vesa
SRCS=	opt_vga.h opt_vesa.h
SRCS+=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/fb
SRCS+=	vesa.c

.PATH:	${.CURDIR}/../../dev/syscons
SRCS+=	scvesactl.c

.include <bsd.kmod.mk>
