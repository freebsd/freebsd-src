# $FreeBSD: src/sys/modules/ie/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#

.PATH:	${.CURDIR}/../../dev/ie

KMOD=	if_ie
SRCS=	if_ie.c if_ie_isa.c \
	isa_if.h bus_if.h device_if.h
WERROR=

.include <bsd.kmod.mk>
