# $FreeBSD: src/sys/modules/acpi/Makefile,v 1.53.12.1 2010/12/21 17:10:29 kensmith Exp $

.if ${MACHINE} == "i386"
SUBDIR=		acpi
.endif

SUBDIR+=	acpi_aiboost acpi_asus acpi_fujitsu acpi_ibm		\
		acpi_panasonic acpi_sony acpi_toshiba acpi_video	\
		acpi_dock

.include <bsd.subdir.mk>
