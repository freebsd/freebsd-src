# $DragonFly: src/sys/dev/netif/et/Makefile,v 1.1 2007/10/12 14:12:42 sephe Exp $
# $FreeBSD: src/sys/modules/et/Makefile,v 1.1.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/et

KMOD=	if_et
SRCS=	if_et.c
SRCS+=	opt_et.h bus_if.h pci_if.h device_if.h miibus_if.h

.include <bsd.kmod.mk>
