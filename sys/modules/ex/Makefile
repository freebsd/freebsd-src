# $FreeBSD: src/sys/modules/ex/Makefile,v 1.2.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#

.PATH:	${.CURDIR}/../../dev/ex

KMOD=	if_ex
SRCS=	if_ex.c \
	if_ex_isa.c isa_if.h \
	if_ex_pccard.c card_if.h \
	bus_if.h device_if.h pccarddevs.h

.include <bsd.kmod.mk>
