# $FreeBSD: src/sys/modules/ukbd/Makefile,v 1.16 2004/06/27 12:33:18 imp Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	ukbd
SRCS=	bus_if.h device_if.h vnode_if.h \
	opt_usb.h opt_kbd.h opt_ukbd.h \
	ukbd.c usbdevs.h

opt_kbd.h:
	echo "#define KBD_INSTALL_CDEV 1" > ${.TARGET}

.include <bsd.kmod.mk>
