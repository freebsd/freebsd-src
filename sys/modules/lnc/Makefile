# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/lnc

KMOD=	if_lnc
SRCS=	if_lnc.c if_lnc_pci.c if_lnc_isa.c if_lnc_pc98.c
SRCS+=	opt_inet.h device_if.h bus_if.h isa_if.h pci_if.h
#SRCS+=	miibus_if.h
#KMODDEPS = miibus

CFLAGS+= -g -I${.CURDIR}/../../dev/lnc

.include <bsd.kmod.mk>
