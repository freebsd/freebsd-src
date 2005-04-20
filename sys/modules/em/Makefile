#$FreeBSD$

.PATH: ${.CURDIR}/../../dev/em

KMOD=	if_em
SRCS=	if_em.c if_em_hw.c
SRCS+=	device_if.h bus_if.h pci_if.h opt_bdg.h opt_carp.h

.include <bsd.kmod.mk>
