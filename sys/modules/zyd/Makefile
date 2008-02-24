# ZyDAS Loadable Kernel Module
#
# $FreeBSD: src/sys/modules/zyd/Makefile,v 1.1 2007/08/29 21:04:26 imp Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD    = if_zyd
SRCS    = if_zyd.c opt_usb.h device_if.h bus_if.h usbdevs.h

.include <bsd.kmod.mk>
