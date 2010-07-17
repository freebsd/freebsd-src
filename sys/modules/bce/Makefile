# $FreeBSD: src/sys/modules/bce/Makefile,v 1.1.16.1.4.1 2010/06/14 02:09:06 kensmith Exp $
.PATH:	${.CURDIR}/../../dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

.include <bsd.kmod.mk>
