# $Id: Makefile,v 1.6 2003/10/24 15:43:24 simokawa Exp $
# $FreeBSD: src/sys/modules/dcons_crom/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/dcons

KMOD	= dcons_crom
SRCS	= dcons_crom.c dcons.h dcons_os.h \
		bus_if.h device_if.h

#KMODDEPS = firewire dcons

CFLAGS+= -I${.CURDIR}/../..

.include <bsd.kmod.mk>
