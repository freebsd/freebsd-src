# $FreeBSD: src/sys/modules/el/Makefile,v 1.2 2001/01/06 13:59:45 obrien Exp $

.PATH: ${.CURDIR}/../../i386/isa

KMOD=	if_el
SRCS=	if_el.c
SRCS+=	opt_bdg.h opt_inet.h opt_ipx.h device_if.h bus_if.h isa_if.h

.include <bsd.kmod.mk>
