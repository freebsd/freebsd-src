# $FreeBSD: src/sys/modules/fxp/Makefile,v 1.9.2.2 2001/07/25 17:46:03 jlemon Exp $

.PATH:	${.CURDIR}/../../dev/fxp
KMOD	= if_fxp
SRCS	= if_fxp.c opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h miibus_if.h
KMODDEPS = miibus

CLEANFILES = vlan.h
vlan.h:
	echo '#define NVLAN 1' > vlan.h

.include <bsd.kmod.mk>
