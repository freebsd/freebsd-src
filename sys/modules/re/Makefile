# $FreeBSD: src/sys/modules/re/Makefile,v 1.3.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/re

KMOD=	if_re
SRCS=	if_re.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
