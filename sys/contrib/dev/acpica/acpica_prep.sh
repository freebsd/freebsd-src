#!/bin/sh
# $FreeBSD$
#
# Unpack an ACPI CA drop and restructure it to fit the FreeBSD layout
#

src=$1
wrk=./_acpi_ca_unpack
dst=./acpi_ca_destination

# files to remove
stripdirs="compiler"
stripfiles="osunixxf.c 16bit.h Makefile a16find.c a16utils.asm a16utils.obj\
    acintel.h aclinux.h acmsvc.h acwin.h acwin64.h getopt.c"

# pre-clean
echo pre-clean
rm -rf ${wrk}
rm -rf ${dst}
mkdir -p ${wrk}
mkdir -p ${dst}

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

# post-clean
echo post-clean
rm -rf ${wrk}