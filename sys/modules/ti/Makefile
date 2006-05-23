# $FreeBSD: src/sys/modules/ti/Makefile,v 1.13 2002/06/26 03:34:49 ken Exp $

.PATH: ${.CURDIR}/../../pci

KMOD=	if_ti
SRCS=	if_ti.c opt_bdg.h device_if.h bus_if.h pci_if.h opt_ti.h opt_zero.h \
	vnode_if.h

.include <bsd.kmod.mk>
