# $FreeBSD: src/sys/modules/bfe/Makefile,v 1.1.4.3 2004/02/17 20:43:16 julian Exp $

.PATH: ${.CURDIR}/../../dev/bfe

KMOD=	if_bfe
SRCS=	if_bfe.c miibus_if.h opt_bdg.h device_if.h bus_if.h pci_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
