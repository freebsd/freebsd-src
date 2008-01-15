# $FreeBSD: src/sys/modules/linsysfs/Makefile,v 1.1.2.1 2006/06/15 15:54:59 ambrisko Exp $

.PATH: ${.CURDIR}/../../compat/linsysfs

KMOD=	linsysfs
SRCS=	vnode_if.h \
	device_if.h bus_if.h  pci_if.h \
	linsysfs.c \
	opt_compat.h

.include <bsd.kmod.mk>
