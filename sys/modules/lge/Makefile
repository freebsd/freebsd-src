# $FreeBSD: src/sys/modules/lge/Makefile,v 1.1.2.1 2001/06/19 19:42:41 wpaul Exp $

.PATH: ${.CURDIR}/../../dev/lge

KMOD=	if_lge
SRCS=	if_lge.c miibus_if.h opt_bdg.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
