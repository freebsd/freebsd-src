# $FreeBSD: src/sys/modules/rp/Makefile,v 1.3.2.1 2002/06/18 03:11:46 obrien Exp $

.PATH: ${.CURDIR}/../../dev/rp

KMOD=	rp
SRCS=	rp.c rp_pci.c device_if.h bus_if.h pci_if.h
CFLAGS+= -g

.include <bsd.kmod.mk>
