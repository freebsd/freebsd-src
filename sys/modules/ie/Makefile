# $FreeBSD: src/sys/modules/ie/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $
#

.PATH:	${.CURDIR}/../../dev/ie

KMOD=	if_ie
SRCS=	if_ie.c if_ie_isa.c \
	isa_if.h bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
