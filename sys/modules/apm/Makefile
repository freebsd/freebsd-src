# $FreeBSD: src/sys/modules/apm/Makefile,v 1.7 2003/03/25 05:19:17 mdodd Exp $

.PATH: ${.CURDIR}/../../i386/bios

KMOD=	apm
SRCS=	apm.c apm.h \
	device_if.h bus_if.h

EXPORT_SYMS=	apm_display	\
		apm_softc

.include <bsd.kmod.mk>
