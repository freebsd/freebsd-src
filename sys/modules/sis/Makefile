# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_sis
SRCS	= if_sis.c opt_bdg.h device_if.h bus_if.h pci_if.h
SRCS	+= miibus_if.h

.include "${.CURDIR}/../../conf/kmod.mk"
