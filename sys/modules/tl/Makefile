# $FreeBSD: src/sys/modules/tl/Makefile,v 1.11 2000/01/28 11:26:39 bde Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_tl
SRCS	= if_tl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
