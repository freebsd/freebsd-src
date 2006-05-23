# $FreeBSD: src/sys/modules/udbp/Makefile,v 1.7.2.1 2005/06/07 07:49:49 glebius Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	udbp
SRCS=	bus_if.h device_if.h \
	opt_usb.h opt_netgraph.h \
	udbp.c usbdevs.h

.include <bsd.kmod.mk>
