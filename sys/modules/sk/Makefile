# $FreeBSD: src/sys/modules/sk/Makefile,v 1.14.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
