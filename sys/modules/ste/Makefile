# $FreeBSD: src/sys/modules/ste/Makefile,v 1.7 2000/01/28 11:26:37 bde Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_ste
SRCS	= if_ste.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
