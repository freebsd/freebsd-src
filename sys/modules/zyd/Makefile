# ZyDAS Loadable Kernel Module
#
# $FreeBSD: src/sys/modules/zyd/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD    = if_zyd
SRCS    = if_zyd.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
