# $FreeBSD: src/sys/modules/ie/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $
#

.PATH:	${.CURDIR}/../../dev/ie

KMOD=	if_ie
SRCS=	if_ie.c if_ie_isa.c \
	isa_if.h bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
