# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/tpm

KMOD=	tpm
SRCS=	tpm.c tpm_isa.c tpm_acpi.c isa_if.h opt_acpi.h acpi_if.h \
	bus_if.h device_if.h

.include <bsd.kmod.mk>
