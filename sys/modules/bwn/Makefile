# $FreeBSD: src/sys/modules/bwn/Makefile,v 1.1.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/bwn

KMOD=	if_bwn
SRCS=	if_bwn.c if_bwnreg.h if_bwnvar.h
SRCS+=	device_if.h bus_if.h pci_if.h

.include <bsd.kmod.mk>
