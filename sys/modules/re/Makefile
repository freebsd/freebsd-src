# $FreeBSD: src/sys/modules/re/Makefile,v 1.3.14.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/re

KMOD=	if_re
SRCS=	if_re.c device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
