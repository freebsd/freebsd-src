# $FreeBSD: src/sys/modules/pcn/Makefile,v 1.1.2.1 2000/10/03 18:46:56 wpaul Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_pcn
SRCS	= if_pcn.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
