# $FreeBSD$

.PATH:  ${.CURDIR}/../../dev/pcic ${.CURDIR}/../../dev/pccard \
	${.CURDIR}/../../pci
KMOD=   pcic
SRCS=   i82365.c i82365_isa.c device_if.h bus_if.h isa_if.h \
	pcic_p.c pci_if.h power_if.h card_if.h power_if.c card_if.c
# pcic_p above is a kludge.
NOMAN=

.include <bsd.kmod.mk>
