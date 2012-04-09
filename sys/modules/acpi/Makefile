# $FreeBSD: src/sys/modules/acpi/Makefile,v 1.54.2.2.4.1 2012/03/03 06:15:13 kensmith Exp $

.if ${MACHINE} == "i386"
SUBDIR=		acpi
.endif

SUBDIR+=	acpi_aiboost acpi_asus acpi_fujitsu acpi_hp acpi_ibm	\
		acpi_panasonic acpi_sony acpi_toshiba acpi_video	\
		acpi_dock acpi_wmi aibs

.include <bsd.subdir.mk>
