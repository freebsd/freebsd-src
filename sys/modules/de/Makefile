# $FreeBSD: src/sys/modules/de/Makefile,v 1.7.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/de

KMOD=	if_de
SRCS=	if_de.c opt_ddb.h device_if.h bus_if.h pci_if.h

# Compile-time options.

#  Enable debugging:
#CFLAGS+=	-DTULIP_DEBUG

#  Enable performance statistics:
#CFLAGS+=	-DTULIP_PERFSTATS

.include <bsd.kmod.mk>
