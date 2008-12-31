# $FreeBSD: src/sys/modules/acpi/acpi_ibm/Makefile,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:		${.CURDIR}/../../../dev/acpi_support
KMOD=		acpi_ibm
CFLAGS+=	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=		acpi_ibm.c opt_acpi.h device_if.h bus_if.h acpi_if.h
SRCS+=		opt_ddb.h

.include <bsd.kmod.mk>
