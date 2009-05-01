# $FreeBSD: src/sys/modules/bce/Makefile,v 1.1.14.1 2009/04/15 03:14:26 kensmith Exp $
.PATH:	${.CURDIR}/../../dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

.include <bsd.kmod.mk>
