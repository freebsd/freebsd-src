# $FreeBSD: src/sys/modules/uscanner/Makefile,v 1.1.2.2 2004/03/01 00:07:26 julian Exp $ 

MAINTAINER=	n_hibma@freebsd.org

.PATH: ${.CURDIR}/../../dev/usb

KMOD=	 uscanner
SRCS=	bus_if.h device_if.h vnode_if.h opt_usb.h uscanner.c

CFLAGS+= -g

.include <bsd.kmod.mk>
