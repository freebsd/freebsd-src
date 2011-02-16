# ZyDAS Loadable Kernel Module
#
# $FreeBSD: src/sys/modules/zyd/Makefile,v 1.1.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD    = if_zyd
SRCS    = if_zyd.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
