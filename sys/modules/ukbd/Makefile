# $FreeBSD: src/sys/modules/ukbd/Makefile,v 1.18.2.1.8.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	ukbd
SRCS=	bus_if.h device_if.h opt_usb.h opt_kbd.h opt_ukbd.h usbdevs.h ukbd.c

.if !defined(KERNBUILDDIR)
opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
