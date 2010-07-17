# $FreeBSD: src/sys/modules/sk/Makefile,v 1.14.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
