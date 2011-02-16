# $FreeBSD: src/sys/modules/acpi/acpi_fujitsu/Makefile,v 1.1.24.1 2010/12/21 17:10:29 kensmith Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_fujitsu
CFLAGS+=-I${.CURDIR}/../../../dev/acpi_support \
	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_fujitsu.c opt_acpi.h acpi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
