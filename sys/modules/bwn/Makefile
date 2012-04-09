# $FreeBSD: src/sys/modules/bwn/Makefile,v 1.1.2.2.6.1 2012/03/03 06:15:13 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bwn

KMOD=	if_bwn
SRCS=	if_bwn.c if_bwnreg.h if_bwnvar.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
