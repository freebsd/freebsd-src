# $FreeBSD$

S	= ${.CURDIR}/../..
.PATH:	$S/dev/usb
KMOD	= if_aue
SRCS	= if_aue.c opt_bdg.h opt_usb.h device_if.h bus_if.h
SRCS	+= miibus_if.h
KMODDEPS = miibus

.include <bsd.kmod.mk>
