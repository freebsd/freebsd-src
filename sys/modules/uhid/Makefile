#       $FreeBSD: src/sys/modules/uhid/Makefile,v 1.7 1999/11/28 18:53:30 bde Exp $ 

MAINTAINER	 = n_hibma@freebsd.org

.PATH:  	 ${.CURDIR}/../../dev/usb
KMOD		 = uhid
SRCS		 = bus_if.h device_if.h vnode_if.h opt_usb.h uhid.c
NOMAN		 =

.include <bsd.kmod.mk>
