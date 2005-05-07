# $FreeBSD: src/sys/modules/apm/Makefile,v 1.7.10.1 2005/05/01 06:13:31 nyan Exp $

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
