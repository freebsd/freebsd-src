# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/si

KMOD=	si
SRCS=	si.c si2_z280.c si3_t225.c si_eisa.c si_isa.c si_pci.c
SRCS+=	bus_if.h device_if.h eisa_if.h isa_if.h pci_if.h
SRCS+=	opt_compat.h opt_debug_si.h opt_eisa.h

.include <bsd.kmod.mk>
