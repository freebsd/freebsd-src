# $FreeBSD: src/sys/modules/ti/Makefile,v 1.8 2000/01/28 11:26:38 bde Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_ti
SRCS	= if_ti.c opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
CLEANFILES	= vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>
