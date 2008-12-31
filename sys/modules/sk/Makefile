# $FreeBSD: src/sys/modules/sk/Makefile,v 1.14.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
