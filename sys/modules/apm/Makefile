# $FreeBSD$

.if ${MACHINE} == "pc98"
.PATH: ${.CURDIR}/../../pc98/apm
CFLAGS+= -DPC98
.else
.PATH: ${.CURDIR}/../../i386/apm
.endif

KMOD=	apm
SRCS=	apm.c apm.h \
	device_if.h bus_if.h
NOMAN=

.include <bsd.kmod.mk>
