# $FreeBSD: src/sys/modules/ie/Makefile,v 1.2 2005/01/26 16:26:06 imp Exp $
#

.PATH:	${.CURDIR}/../../dev/ie

KMOD=	if_ie
SRCS=	if_ie.c if_ie_isa.c \
	isa_if.h bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
