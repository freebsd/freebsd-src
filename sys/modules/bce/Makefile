# $FreeBSD: src/sys/modules/bce/Makefile,v 1.1.16.1.6.1 2010/12/21 17:09:25 kensmith Exp $
.PATH:	${.CURDIR}/../../dev/bce
KMOD=	if_bce
SRCS=	opt_bce.h if_bce.c miibus_if.h miidevs.h device_if.h bus_if.h pci_if.h

#CFLAGS += -DBCE_DEBUG=0

.include <bsd.kmod.mk>
