#!/bin/sh
# $FreeBSD$
#
echo 'static char wakecode[] = {';
hexdump -Cv acpi_wakecode.bin | \
    sed -e 's/^[0-9a-f][0-9a-f]*//' -e 's/\|.*$//' | \
    while read line
    do
	for code in ${line}
	do
	    echo -n "0x${code},";
	done
    done
echo '};'

nm -n acpi_wakecode.o | while read offset dummy what
do
    echo "#define ${what}	0x${offset}"
done

exit 0
