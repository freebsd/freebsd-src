# $Id: Makefile,v 1.6 2003/10/24 15:43:24 simokawa Exp $
# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h \
		bus_if.h device_if.h

#KMODDEPS = firewire dcons

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
