# $FreeBSD: src/sys/modules/re/Makefile,v 1.2 2003/09/08 03:24:29 wpaul Exp $

.PATH: ${.CURDIR}/../../dev/re

KMOD=	if_re
SRCS=	if_re.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS+=	miibus_if.h

.include <bsd.kmod.mk>
