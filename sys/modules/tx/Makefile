# $FreeBSD: src/sys/modules/tx/Makefile,v 1.4.2.2 2001/09/01 07:17:11 semenu Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_tx
SRCS	= if_tx.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h vlan.h
CLEANFILES = vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>
