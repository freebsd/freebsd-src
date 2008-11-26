# $FreeBSD: src/sys/modules/linsysfs/Makefile,v 1.1.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../compat/linsysfs

KMOD=	linsysfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	linsysfs.c \
	opt_compat.h

.include <bsd.kmod.mk>
