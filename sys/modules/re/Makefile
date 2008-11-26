# $FreeBSD: src/sys/modules/re/Makefile,v 1.2.22.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/re

KMOD=	if_re
SRCS=	if_re.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
