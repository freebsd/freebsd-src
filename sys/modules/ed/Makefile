# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/ed
KMOD	= if_ed
SRCS	= if_ed.c if_ed_cbus.c if_ed_isa.c if_ed_pccard.c if_ed_pci.c
SRCS   += opt_bdg.h device_if.h bus_if.h pci_if.h isa_if.h

.include <bsd.kmod.mk>
