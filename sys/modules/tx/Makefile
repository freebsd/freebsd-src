# $FreeBSD: src/sys/modules/tx/Makefile,v 1.4.2.3 2001/12/04 20:01:54 brooks Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_tx
SRCS	= if_tx.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h

.include <bsd.kmod.mk>
