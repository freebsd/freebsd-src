# $FreeBSD: src/sys/modules/apm/Makefile,v 1.5 2002/11/06 13:46:58 nyan Exp $

.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/apm
.else
.PATH: ${.CURDIR}/../../i386/apm
.endif

KMOD=	apm
SRCS=	apm.c apm.h \
	device_if.h bus_if.h

EXPORT_SYMS=	apm_display	\
		apm_softc

.include <bsd.kmod.mk>
