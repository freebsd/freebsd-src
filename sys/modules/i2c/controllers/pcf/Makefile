# $FreeBSD$

.PATH:		${.CURDIR}/../../../../dev/pcf
KMOD		= pcf
SRCS		= device_if.h bus_if.h iicbus_if.h isa_if.h \
		  pcf.c

.if ${MACHINE_ARCH} != "sparc64"
SRCS		+= pcf_isa.c
.endif

.if ${MACHINE_ARCH} == "sparc64"
SRCS		+= envctrl.c
SRCS		+= pcf_ebus.c
.endif

.include <bsd.kmod.mk>
