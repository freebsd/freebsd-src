# $FreeBSD: src/sys/modules/sk/Makefile,v 1.12.14.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
