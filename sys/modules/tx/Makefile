# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_tx
SRCS	= if_tx.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h

.include <bsd.kmod.mk>
