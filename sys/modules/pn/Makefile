# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_pn
SRCS	= if_pn.c opt_bdg.h device_if.h bus_if.h pci_if.h
CFLAGS	+= ${DEBUG_FLAGS}

.include <bsd.kmod.mk>
