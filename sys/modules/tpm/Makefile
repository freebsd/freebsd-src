# $FreeBSD: src/sys/modules/tpm/Makefile,v 1.2.2.2.2.1 2010/12/21 17:09:25 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/tpm

KMOD=	tpm

SRCS=	tpm.c bus_if.h device_if.h
#Bus specific stuff.
SRCS+=	tpm_isa.c tpm_acpi.c isa_if.h opt_acpi.h acpi_if.h

.include <bsd.kmod.mk>
