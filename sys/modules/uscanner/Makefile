# $FreeBSD: src/sys/modules/uscanner/Makefile,v 1.3 2002/01/11 15:49:01 ru Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	 uscanner
SRCS=	bus_if.h device_if.h vnode_if.h opt_usb.h uscanner.c

CFLAGS+= -g

.include <bsd.kmod.mk>
