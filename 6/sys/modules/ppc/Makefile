# $FreeBSD$

#.PATH:	${.CURDIR}/../../kern
.PATH:	${.CURDIR}/../../dev/ppc
.PATH:	${.CURDIR}/../../dev/ppbus

KMOD=	ppc
SRCS=	bus_if.h ppbus_if.h opt_ppc.h device_if.h isa_if.h \
	ppc.c ppc_puc.c

.include <bsd.kmod.mk>
