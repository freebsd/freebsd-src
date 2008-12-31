# $FreeBSD: src/sys/modules/ed/Makefile,v 1.11.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../dev/ed

KMOD=	if_ed
SRCS=	if_ed.c
SRCS+=	if_ed_novell.c if_ed_wd80x3.c if_ed_rtl80x9.c isa_if.h
.if ${MACHINE} == "pc98"
SRCS+=	if_ed_cbus.c
.else
SRCS+=	if_ed_isa.c if_ed_3c503.c if_ed_hpp.c if_ed_sic.c
.endif
SRCS+=	if_ed_pccard.c pccarddevs.h card_if.h
SRCS+=	if_ed_pci.c pci_if.h

SRCS+=	opt_ed.h bus_if.h device_if.h miibus_if.h

.include <bsd.kmod.mk>
