#!/bin/sh
# $FreeBSD: src/sys/i386/acpica/genwakecode.sh,v 1.2 2004/07/27 01:33:27 tjr Exp $
#
file2c 'static char wakecode[] = {' '};' <acpi_wakecode.bin

nm -n acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
