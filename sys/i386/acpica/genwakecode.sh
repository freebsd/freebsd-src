#!/bin/sh
# $FreeBSD: src/sys/i386/acpica/genwakecode.sh,v 1.3.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
file2c 'static char wakecode[] = {' '};' <acpi_wakecode.bin

nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
