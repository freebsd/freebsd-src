# $FreeBSD: src/sys/modules/re/Makefile,v 1.3.8.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/re

KMOD=	if_re
SRCS=	if_re.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
