# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_wb
SRCS	= if_wb.c opt_bdg.h device_if.h
SRCS	+= bus_if.h miibus_if.h pci_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
