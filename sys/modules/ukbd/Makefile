# $FreeBSD: src/sys/modules/ukbd/Makefile,v 1.22.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	ukbd
SRCS=	bus_if.h device_if.h opt_compat.h opt_usb.h opt_kbd.h opt_ukbd.h \
	usbdevs.h ukbd.c

.if !defined(KERNBUILDDIR)
opt_compat.h:
	echo "#define COMPAT_FREEBSD6 1" > ${.TARGET}

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
