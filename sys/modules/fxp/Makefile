# $FreeBSD: src/sys/modules/fxp/Makefile,v 1.9.2.3 2001/12/04 20:01:53 brooks Exp $

.PATH:	${.CURDIR}/../../dev/fxp
KMOD	= if_fxp
SRCS	= if_fxp.c opt_bdg.h device_if.h bus_if.h pci_if.h miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
