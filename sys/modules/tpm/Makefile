# $FreeBSD: src/sys/modules/tpm/Makefile,v 1.2.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

.PATH:	${.CURDIR}/../../dev/tpm

KMOD=	tpm

SRCS=	tpm.c bus_if.h device_if.h
#Bus specific stuff.
SRCS+=	tpm_isa.c tpm_acpi.c isa_if.h opt_acpi.h acpi_if.h

.include <bsd.kmod.mk>
