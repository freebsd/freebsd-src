# $Id: Makefile,v 1.6 2003/10/24 15:43:24 simokawa Exp $
# $FreeBSD: src/sys/modules/dcons_crom/Makefile,v 1.1.2.2 2003/11/26 08:42:39 simokawa Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h \
		bus_if.h device_if.h

KMODDEPS = firewire dcons

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
