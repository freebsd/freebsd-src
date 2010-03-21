# ZyDAS Loadable Kernel Module
#
# $FreeBSD: src/sys/modules/zyd/Makefile,v 1.1.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD    = if_zyd
SRCS    = if_zyd.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
