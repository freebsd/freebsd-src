# $FreeBSD: src/sys/modules/dc/Makefile,v 1.2 2000/01/28 11:26:28 bde Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_dc
SRCS	= if_dc.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
