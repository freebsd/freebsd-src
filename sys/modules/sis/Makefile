# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_sis
SRCS	= if_sis.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
CFLAGS	+= ${DEBUG_FLAGS}
KMODDEPS = miibus

.include <bsd.kmod.mk>
