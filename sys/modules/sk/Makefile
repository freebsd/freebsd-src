# $FreeBSD: src/sys/modules/sk/Makefile,v 1.12.14.1 2006/06/13 01:39:47 yongari Exp $

.PATH: ${.CURDIR}/../../dev/sk

KMOD=	if_sk
SRCS=	if_sk.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
