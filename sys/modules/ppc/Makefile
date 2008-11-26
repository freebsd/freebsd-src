# $FreeBSD: src/sys/modules/ppc/Makefile,v 1.3.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

#.PATH:	${.CURDIR}/../../kern
.PATH:	${.CURDIR}/../../dev/ppc
.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	ppc
SRCS=	bus_if.h ppbus_if.h opt_ppc.h device_if.h isa_if.h \
	ppc.c ppc_puc.c

.include <bsd.kmod.mk>
