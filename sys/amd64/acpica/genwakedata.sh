#!/bin/sh
# $FreeBSD: src/sys/amd64/acpica/genwakedata.sh,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#
nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
