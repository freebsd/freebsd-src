# $FreeBSD: src/sys/modules/vesa/Makefile,v 1.14.36.2.6.1 2012/03/03 06:15:13 kensmith Exp $

KMOD=	vesa
SRCS=	opt_vga.h opt_vesa.h
SRCS+=	bus_if.h device_if.h pci_if.h

.PATH:	${.CURDIR}/../../dev/fb
SRCS+=	vesa.c

.PATH:	${.CURDIR}/../../dev/syscons
SRCS+=	scvesactl.c

.include <bsd.kmod.mk>
