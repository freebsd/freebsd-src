#!/bin/sh
# $FreeBSD: src/sys/i386/acpica/genwakecode.sh,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
#
file2c 'static char wakecode[] = {' '};' <acpi_wakecode.bin

nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
