# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_rl
SRCS	= if_rl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

CFLAGS	+= ${DEBUG_FLAGS}

.include <bsd.kmod.mk>
