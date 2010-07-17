# $FreeBSD: src/sys/modules/cmx/Makefile,v 1.1.4.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
MAINTAINER=	daniel@roe.ch

.PATH:	${.CURDIR}/../../dev/cmx

KMOD=	cmx
SRCS=	cmx.c cmxvar.h cmxreg.h \
	cmx_pccard.c \
	pccarddevs.h device_if.h bus_if.h card_if.h

.include <bsd.kmod.mk>
