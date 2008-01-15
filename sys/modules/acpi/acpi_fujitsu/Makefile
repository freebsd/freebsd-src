# $FreeBSD: src/sys/modules/acpi/acpi_fujitsu/Makefile,v 1.1 2005/03/18 08:48:10 philip Exp $

.PATH:	${.CURDIR}/../../../dev/acpi_support

KMOD=	acpi_fujitsu
CFLAGS+=-I${.CURDIR}/../../../dev/acpi_support \
	-I${.CURDIR}/../../../contrib/dev/acpica
SRCS=	acpi_fujitsu.c opt_acpi.h acpi_if.h device_if.h bus_if.h

.include <bsd.kmod.mk>
