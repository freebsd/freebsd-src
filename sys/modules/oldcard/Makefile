# $FreeBSD$

.PATH: ${.CURDIR}/../../pccard ${.CURDIR}/../../pci

KMOD=	oldcard
SRCS=	pccard.c pcic.c pccard_beep.c pccard_nbk.c pcic_p.c \
	opt_pcic.h device_if.h bus_if.h isa_if.h pci_if.h \
	power_if.h card_if.h power_if.c card_if.c
NOMAN=

opt_pcic.h:
	touch $@

.include <bsd.kmod.mk>
