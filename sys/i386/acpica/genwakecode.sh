#!/bin/sh
# $FreeBSD: src/sys/i386/acpica/genwakecode.sh,v 1.2.2.1 2004/12/19 20:34:13 njl Exp $
#
file2c 'static char wakecode[] = {' '};' <acpi_wakecode.bin

nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
