# $FreeBSD$

.PATH: ${.CURDIR}/../../pci

KMOD=	if_wx
KMODDEPS = miibus
SRCS=	if_wx.c opt_bdg.h device_if.h bus_if.h pci_if.h miibus_if.h

.include <bsd.kmod.mk>
