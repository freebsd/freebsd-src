# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_ti
SRCS	= if_ti.c opt_bdg.h vlan.h device_if.h bus_if.h pci_if.h
CLEANFILES	= vlan.h

vlan.h:
	touch vlan.h

.include <bsd.kmod.mk>
