# $FreeBSD: src/sys/modules/bce/Makefile,v 1.1.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $
.PATH:	${.CURDIR}/../../dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

.include <bsd.kmod.mk>
