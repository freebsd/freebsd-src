# $FreeBSD: src/sys/modules/rue/Makefile,v 1.1.4.1 2003/07/30 13:57:36 akiyama Exp $

S	= ${.CURDIR}/../..
.PATH:	$S/dev/usb
KMOD	= if_rue
SRCS	= if_rue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
