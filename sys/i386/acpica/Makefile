# $FreeBSD: src/sys/i386/acpica/Makefile,v 1.6 2004/04/13 13:43:11 des Exp $

# Correct path for kernel builds
# Don't rely on the kernel's .depend file
.ifdef MAKESRCPATH
.PATH: ${MAKESRCPATH}
DEPENDFILE=
.else
MAKESRCPATH= ${.CURDIR}
CLEANFILES= acpi_wakecode.h acpi_wakecode.bin acpi_wakecode.o
.endif
CFLAGS+=	-I. -I@

all: acpi_wakecode.h

acpi_wakecode.o: acpi_wakecode.S

acpi_wakecode.bin: acpi_wakecode.o
	objcopy -S -O binary acpi_wakecode.o acpi_wakecode.bin

acpi_wakecode.h: acpi_wakecode.bin acpi_wakecode.o
	sh ${MAKESRCPATH}/genwakecode.sh > acpi_wakecode.h

.include <bsd.prog.mk>
