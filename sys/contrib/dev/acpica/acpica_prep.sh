#!/bin/sh
# $FreeBSD: src/sys/contrib/dev/acpica/acpica_prep.sh,v 1.10.6.1 2008/11/25 02:59:29 kensmith Exp $
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

# files that should keep their full directory path
fulldirs="common compiler"
# files to remove
stripdirs="generate acpisrc"
stripfiles="16bit.h Makefile README a16find.c a16utils.asm a16utils.obj	\
	acdos16.h acintel.h aclinux.h acmsvc.h acnetbsd.h acpixtract.c	\
	acwin.h acwin64.h aeexec.c aemain.c osdosxf.c osunixdir.c	\
	oswindir.c oswinxf.c readme.txt"
# include files to canonify
src_headers="acapps.h acconfig.h acdebug.h acdisasm.h acdispat.h	\
	acenv.h	acevents.h acexcep.h acfreebsd.h acgcc.h acglobal.h	\
	achware.h acinterp.h aclocal.h acmacros.h acnames.h acnamesp.h	\
	acobject.h acopcode.h acoutput.h acparser.h acpi.h acpiosxf.h	\
	acpixf.h acresrc.h acstruct.h actables.h actbl.h actbl1.h	\
	actbl2.h actypes.h acutils.h aecommon.h amlcode.h amlresrc.h"
comp_headers="aslcompiler.h asldefine.h aslglobal.h asltypes.h"
	
# files to update paths in
src_update_files="acpi.h acpiosxf.h"

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

echo copying full dirs
for i in ${fulldirs}; do
	find ${wrk} -name ${i} -type d | xargs -J % mv % ${dst}
done

# move files to destination
echo copying flat dirs
find ${wrk} -type f | xargs -J % mv % ${dst}
mv ${dst}/changes.txt ${dst}/CHANGES.txt

# update src/headers for appropriate paths
echo updating paths
for i in ${src_update_files}; do
	i=${dst}/$i
	sed -e 's/platform\///' $i > $i.new && mv $i.new $i
done

# canonify include paths
for H in ${src_headers}; do
	find ${dst} -name "*.[chy]" -type f |	\
	xargs sed -i "" -e "s|[\"<]$H[\">]|\<contrib/dev/acpica/$H\>|g"
done
for H in ${comp_headers}; do
	find ${dst}/compiler -name "*.[chly]" -type f |	\
	xargs sed -i "" -e "s|[\"<]$H[\">]|\<contrib/dev/acpica/compiler/$H\>|g"
done

# post-clean
echo post-clean
rm -rf ${wrk}

# assist the developer in generating a diff
echo "Directories you may want to 'cvs diff':"
echo "    src/sys/contrib/dev/acpica src/sys/dev/acpica \\"
echo "    src/sys/amd64/acpica src/sys/i386/acpica src/sys/ia64/acpica \\"
echo "    src/sys/amd64/include src/sys/i386/include src/sys/ia64/include \\"
echo "    src/sys/boot src/sys/conf src/sys/modules/acpi src/usr.sbin/acpi"
