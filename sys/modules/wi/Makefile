# $FreeBSD: src/sys/modules/wi/Makefile,v 1.17.8.1 2006/02/10 00:38:08 yar Exp $

.PATH: ${.CURDIR}/../../dev/wi

KMOD=	if_wi
SRCS=	opt_wi.h if_wi.c if_wi_pccard.c if_wi_pci.c \
	card_if.h device_if.h bus_if.h pci_if.h pccarddevs.h

.if !defined(KERNBUILDDIR)
opt_wi.h:
	echo "#define WI_SYMBOL_FIRMWARE 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
