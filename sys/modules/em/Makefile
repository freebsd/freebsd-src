#$FreeBSD: src/sys/modules/em/Makefile,v 1.4 2004/06/09 08:27:05 obrien Exp $

.PATH: ${.CURDIR}/../../dev/em

KMOD=	if_em
SRCS=	if_em.c if_em_hw.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_bdg.h

.include <bsd.kmod.mk>
