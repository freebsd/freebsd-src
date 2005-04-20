# $FreeBSD$

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
