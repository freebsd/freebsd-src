# $FreeBSD$

KMOD=	fdc

.if ${MACHINE} == "pc98"
.PATH:  ${.CURDIR}/../../pc98/cbus
SRCS=	fdc.c fdc_cbus.c
.else
.PATH:  ${.CURDIR}/../../dev/fdc
SRCS=	fdc.c fdc_isa.c fdc_pccard.c
.if ${MACHINE} == "i386" || ${MACHINE} == "amd64"
SRCS+=	opt_acpi.h acpi_if.h fdc_acpi.c
.endif
.endif

SRCS+=	opt_fdc.h bus_if.h card_if.h device_if.h \
	isa_if.h pccarddevs.h

.include <bsd.kmod.mk>
