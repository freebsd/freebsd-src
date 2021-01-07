# $FreeBSD$

KMOD=	fdc

.PATH:  ${SRCTOP}/sys/dev/fdc
SRCS=	fdc.c fdc_isa.c
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
SRCS+=	opt_acpi.h acpi_if.h fdc_acpi.c
.endif

SRCS+=	opt_fdc.h bus_if.h device_if.h isa_if.h

.include <bsd.kmod.mk>
