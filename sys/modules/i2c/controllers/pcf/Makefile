# $FreeBSD: src/sys/modules/i2c/controllers/pcf/Makefile,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../../../../dev/pcf

KMOD=	pcf
SRCS=	${envctrl} pcf.c ${pcf_ebus} ${pcf_isa}
SRCS+=	bus_if.h device_if.h iicbus_if.h ${isa_if} ${ofw_bus_if}

.if ${MACHINE_ARCH} == "i386"
isa_if=		isa_if.h
pcf_isa=	pcf_isa.c
.endif

.if ${MACHINE_ARCH} == "sparc64"
envctrl=	envctrl.c
ofw_bus_if=	ofw_bus_if.h
pcf_ebus=	pcf_ebus.c
.endif

.include <bsd.kmod.mk>
