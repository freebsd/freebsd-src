# $FreeBSD: src/sys/modules/acpi/acpi_asus/Makefile,v 1.8.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_asus
CFLAGS+= -I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_asus.c opt_acpi.h acpi_if.h bus_if.h device_if.h
SRCS+= opt_ddb.h

.include <bsd.kmod.mk>
