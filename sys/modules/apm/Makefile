# $FreeBSD: src/sys/modules/apm/Makefile,v 1.9.16.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../../i386/bios
.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/apm
.endif

KMOD=	apm
SRCS=	apm.c apm.h
.if ${MACHINE} == "pc98"
SRCS+=	apm_bioscall.S
.endif
SRCS+=	device_if.h bus_if.h

EXPORT_SYMS=	apm_display	\
		apm_softc

.include <bsd.kmod.mk>
