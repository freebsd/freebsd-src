# $FreeBSD: src/sys/modules/bge/Makefile,v 1.1.2.2 2001/12/04 20:01:53 brooks Exp $

.PATH: ${.CURDIR}/../../dev/bge

KMOD=	if_bge
SRCS=	if_bge.c miibus_if.h opt_bdg.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
