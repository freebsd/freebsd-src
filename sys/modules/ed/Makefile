# $FreeBSD$

.PATH: ${.CURDIR}/../../dev/ed

KMOD=	if_ed
SRCS=	if_ed.c if_ed_pccard.c if_ed_pci.c

.if ${MACHINE} == "pc98"
SRCS+=	if_ed_cbus.c
.else
SRCS+=	if_ed_isa.c
.endif

SRCS+=	opt_ed.h opt_bdg.h bus_if.h card_if.h device_if.h isa_if.h pci_if.h \
	miibus_if.h

.include <bsd.kmod.mk>
