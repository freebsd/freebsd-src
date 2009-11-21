#!/bin/sh
# $FreeBSD: src/sys/amd64/acpica/genwakecode.sh,v 1.2.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $
#
file2c -sx 'static char wakecode[] = {' '};' <acpi_wakecode.bin

exit 0
