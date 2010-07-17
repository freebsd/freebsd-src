#!/bin/sh
# $FreeBSD: src/sys/amd64/acpica/genwakecode.sh,v 1.2.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $
#
file2c -sx 'static char wakecode[] = {' '};' <acpi_wakecode.bin

exit 0
