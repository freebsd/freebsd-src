# $FreeBSD: src/sys/modules/ie/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $
#

.PATH:	${.CURDIR}/../../dev/ie

KMOD=	if_ie
SRCS=	if_ie.c if_ie_isa.c \
	isa_if.h bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
