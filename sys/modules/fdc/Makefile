# $FreeBSD$

.if ${MACHINE} == "pc98"
.PATH:  ${.CURDIR}/../../pc98/pc98
.else
.PATH:  ${.CURDIR}/../../dev/fdc
.endif
KMOD=	fdc
WARNS?=	2
.if ${MACHINE} == "pc98"
SRCS=	fd.c fdc_cbus.c
.else
CFLAGS+= -I${.CURDIR}/../../contrib/dev/acpica
SRCS=	fdc.c fdc_acpi.c fdc_isa.c fdc_pccard.c
.endif
SRCS+=	opt_acpi.h opt_fdc.h acpi_if.h bus_if.h card_if.h device_if.h \
	isa_if.h pccarddevs.h

FDC_DEBUG?=	0	# 0/1

opt_fdc.h:
	touch ${.TARGET}
.if ${FDC_DEBUG} > 0
	echo "#define FDC_DEBUG 1" >> ${.TARGET}
.endif

.include <bsd.kmod.mk>
