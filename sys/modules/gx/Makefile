# $FreeBSD: src/sys/modules/gx/Makefile,v 1.1.2.2 2001/12/04 20:01:53 brooks Exp $

.PATH:	${.CURDIR}/../../dev/gx
KMOD	= if_gx
SRCS	= if_gx.c opt_bdg.h device_if.h bus_if.h pci_if.h miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
