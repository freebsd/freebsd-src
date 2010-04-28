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
wrk=`realpath ./_acpi_ca_unpack`
dst=`realpath ./acpi_ca_destination`

# files that should keep their full directory path
fulldirs="common compiler debugger disassembler dispatcher events	\
	executer hardware include namespace parser resources tables	\
	tools utilities"

# files to remove
stripdirs="acpisrc acpixtract examples generate os_specific tests"
stripfiles="Makefile README acintel.h aclinux.h acmsvc.h acnetbsd.h	\
	acos2.h accygwin.h acefi.h acwin.h acwin64.h aeexec.c		\
	aehandlers.c aemain.c aetables.c osunixdir.c readme.txt		\
	utclib.c"

# include files to canonify
src_headers="acapps.h accommon.h acconfig.h acdebug.h acdisasm.h	\
	acdispat.h acevents.h acexcep.h acglobal.h achware.h acinterp.h	\
	aclocal.h acmacros.h acnames.h acnamesp.h acobject.h acopcode.h	\
	acoutput.h acparser.h acpi.h acpiosxf.h acpixf.h acpredef.h	\
	acresrc.h acrestyp.h acstruct.h actables.h actbl.h actbl1.h	\
	actbl2.h actypes.h acutils.h amlcode.h amlresrc.h		\
	platform/acenv.h platform/acfreebsd.h platform/acgcc.h"
comp_headers="aslcompiler.h asldefine.h aslglobal.h asltypes.h"
platform_headers="acfreebsd.h acgcc.h"

# pre-clean
echo pre-clean
rm -rf ${wrk} ${dst}
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

# copy files
echo copying full dirs
for i in ${fulldirs}; do
	find ${wrk} -name ${i} -type d | xargs -J % mv % ${dst}
done
echo copying remaining files
find ${wrk} -type f | xargs -J % mv % ${dst}

# canonify include paths
for H in ${src_headers}; do
	find ${dst} -name "*.[chy]" -type f |	\
	xargs sed -i "" -e "s|[\"<]$H[\">]|\<contrib/dev/acpica/include/$H\>|g"
done
for H in ${comp_headers}; do
	find ${dst}/compiler -name "*.[chly]" -type f |	\
	xargs sed -i "" -e "s|[\"<]$H[\">]|\<contrib/dev/acpica/compiler/$H\>|g"
done
for H in ${platform_headers}; do
	find ${dst}/include/platform -name "*.h" -type f |	\
	xargs sed -i "" -e "s|[\"<]$H[\">]|\<contrib/dev/acpica/include/platform/$H\>|g"
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
