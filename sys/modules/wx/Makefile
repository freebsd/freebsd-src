# $FreeBSD: src/sys/modules/wx/Makefile,v 1.1.2.2 2001/04/21 23:39:20 mjacob Exp $

.PATH:	${.CURDIR}/../../pci
KMOD	= if_wx
SRCS	= if_wx.c opt_bdg.h device_if.h bus_if.h pci_if.h miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
