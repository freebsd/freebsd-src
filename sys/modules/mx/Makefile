# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_mx
SRCS	= if_mx.c opt_bdg.h device_if.h bus_if.h pci_if.h miibus_if.h
CFLAGS	+= ${DEBUG_FLAGS}

.include <bsd.kmod.mk>
