# $FreeBSD$

.PATH:	${.CURDIR}/../../pci
KMOD	= if_wx
SRCS	= if_wx.c opt_bdg.h device_if.h bus_if.h pci_if.h miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
