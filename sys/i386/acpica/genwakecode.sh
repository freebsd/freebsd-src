#!/bin/sh
# $FreeBSD: src/sys/i386/acpica/genwakecode.sh,v 1.3.26.1 2010/12/21 17:10:29 kensmith Exp $
#
file2c 'static char wakecode[] = {' '};' <acpi_wakecode.bin

nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
