#!/bin/sh
# $FreeBSD$
#
# Unpack an ACPI CA drop and restructure it to fit the FreeBSD layout
#

if [ ! $# -eq 1 ]; then
	echo "usage: $0 acpica_archive"
	exit
fi

src=$1
wrk=./_acpi_ca_unpack
dst=./acpi_ca_destination

# files to remove
stripdirs="common compiler generate"
stripfiles="osunixxf.c Makefile adisasm.h acdos16.h\
    acintel.h aclinux.h acmsvc.h acwin.h acwin64.h"

# pre-clean
echo pre-clean
rm -rf ${wrk}
rm -rf ${dst}
mkdir -p ${wrk}
mkdir -p ${dst}

# fetch document
echo fetch document
fetch http://developer.intel.com/technology/iapc/acpi/downloads/CHANGES.txt
tr -d '\r' < CHANGES.txt > CHANGES.txt.tmp
mv CHANGES.txt.tmp CHANGES.txt

# unpack
echo unpack
tar -x -z -f ${src} -C ${wrk}

# strip files
echo strip
for i in ${stripdirs}; do
    find ${wrk} -name ${i} -type d | xargs rm -r
done
for i in ${stripfiles}; do
    find ${wrk} -name ${i} -type f -delete
done

# move files to destination
echo copy
find ${wrk} -type f | xargs -J % mv % ${dst}
mv CHANGES.txt ${dst}

# post-clean
echo post-clean
rm -rf ${wrk}
