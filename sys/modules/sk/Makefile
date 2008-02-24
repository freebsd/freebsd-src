# $FreeBSD: src/sys/modules/sk/Makefile,v 1.14 2006/04/27 00:14:02 yongari Exp $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
