# $FreeBSD: src/sys/modules/ukbd/Makefile,v 1.18 2005/06/04 10:58:38 schweikh Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	ukbd
SRCS=	bus_if.h device_if.h opt_usb.h opt_kbd.h opt_ukbd.h usbdevs.h ukbd.c

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}

.include <bsd.kmod.mk>
