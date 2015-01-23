# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/en

KMOD=	if_en
SRCS=	if_en_pci.c midway.c device_if.h bus_if.h pci_if.h
SRCS+=	opt_inet.h opt_natm.h opt_ddb.h
CFLAGS+= -DENABLE_BPF
# CFLAGS+= -DINVARIANT_SUPPORT -DINVARIANTS -DEN_DEBUG=0

.include <bsd.kmod.mk>
