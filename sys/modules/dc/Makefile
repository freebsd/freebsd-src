# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_dc
SRCS	= if_dc.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
