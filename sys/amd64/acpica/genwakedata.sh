#!/bin/sh
# $FreeBSD: src/sys/amd64/acpica/genwakedata.sh,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
nm -n --defined-only acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
