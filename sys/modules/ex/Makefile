# $FreeBSD: src/sys/modules/ex/Makefile,v 1.1 2003/03/29 15:41:44 mdodd Exp $
#

.PATH:	${.CURDIR}/../../dev/ex

KMOD=	if_ex
SRCS=	if_ex.c \
	if_ex_isa.c isa_if.h \
	if_ex_pccard.c card_if.h \
	bus_if.h device_if.h

.include <bsd.kmod.mk>
