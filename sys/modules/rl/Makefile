# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_rl
SRCS	= if_rl.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h

.include "${.CURDIR}/../../conf/kmod.mk"
