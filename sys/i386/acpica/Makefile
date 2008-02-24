# $FreeBSD: src/sys/i386/acpica/Makefile,v 1.8 2006/10/19 05:55:09 ru Exp $

# Correct path for kernel builds
# Don't rely on the kernel's .depend file
.ifdef MAKESRCPATH
.PATH: ${MAKESRCPATH}
DEPENDFILE=
.else
MAKESRCPATH= ${.CURDIR}
CLEANFILES= acpi_wakecode.h acpi_wakecode.bin acpi_wakecode.o
.endif
.if ${CC} == "icc"
CFLAGS+=	-restrict
NOSTDINC=	-X
.else
NOSTDINC=	-nostdinc
.endif
CFLAGS+=	${NOSTDINC} -I. -I${MAKESRCPATH}/../..

all: acpi_wakecode.h

acpi_wakecode.o: acpi_wakecode.S assym.s

acpi_wakecode.bin: acpi_wakecode.o
	objcopy -S -O binary acpi_wakecode.o acpi_wakecode.bin

acpi_wakecode.h: acpi_wakecode.bin acpi_wakecode.o
	sh ${MAKESRCPATH}/genwakecode.sh > acpi_wakecode.h

.include <bsd.prog.mk>
