#!/bin/sh
# $FreeBSD$
#
file2c -sx 'static char wakecode[] = {' '};' <acpi_wakecode.bin

exit 0
