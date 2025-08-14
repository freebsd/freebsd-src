#!/bin/sh
#
# This script generates a "memstick image" (image that can be copied to a
# USB memory stick) from a directory tree.  Note that the script does not
# clean up after itself very well for error conditions on purpose so the
# problem can be diagnosed (full filesystem most likely but ...).
#
# Usage: make-memstick.sh <directory tree or manifest> <image filename>
#
#

set -e

scriptdir=$(dirname $(realpath $0))
. ${scriptdir}/../scripts/tools.subr
. ${scriptdir}/../../tools/boot/install-boot.sh

if [ "$(uname -s)" = "FreeBSD" ]; then
	PATH=/bin:/usr/bin:/sbin:/usr/sbin
	export PATH
fi

if [ $# -ne 2 ]; then
	echo "make-memstick.sh /path/to/directory/or/manifest /path/to/image/file"
	exit 1
fi

MAKEFSARG=${1}

if [ -f ${MAKEFSARG} ]; then
	BASEBITSDIR=`dirname ${MAKEFSARG}`
	METALOG=${MAKEFSARG}
elif [ -d ${MAKEFSARG} ]; then
	BASEBITSDIR=${MAKEFSARG}
	METALOG=
else
	echo "${MAKEFSARG} must exist"
	exit 1
fi

if [ -e ${2} ]; then
	echo "won't overwrite ${2}"
	exit 1
fi

echo '/dev/ufs/FreeBSD_Install / ufs ro,noatime 1 1' > ${BASEBITSDIR}/etc/fstab
echo 'root_rw_mount="NO"' > ${BASEBITSDIR}/etc/rc.conf.local
if [ -n "${METALOG}" ]; then
	metalogfilename=$(mktemp /tmp/metalog.XXXXXX)
	cat ${METALOG} > ${metalogfilename}
	echo "./etc/fstab type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
	echo "./etc/rc.conf.local type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
	MAKEFSARG=${metalogfilename}
fi
${MAKEFS} -D -N ${BASEBITSDIR}/etc -B little -o label=FreeBSD_Install -o version=2 ${2}.part ${MAKEFSARG}
rm ${BASEBITSDIR}/etc/fstab
rm ${BASEBITSDIR}/etc/rc.conf.local
if [ -n "${METALOG}" ]; then
	rm ${metalogfilename}
fi

# Make an ESP in a file.
espfilename=$(mktemp /tmp/efiboot.XXXXXX)
if [ -f "${BASEBITSDIR}/boot/loader_ia32.efi" ]; then
	extra_args="${BASEBITSDIR}/boot/loader_ia32.efi bootia32"
fi
make_esp_file ${espfilename} ${fat32min} ${BASEBITSDIR}/boot/loader.efi bootx64 ${extra_args}

${MKIMG} -s mbr \
    -b ${BASEBITSDIR}/boot/mbr \
    -p efi:=${espfilename} \
    -p freebsd:-"${MKIMG} -s bsd -b ${BASEBITSDIR}/boot/boot -p freebsd-ufs:=${2}.part" \
    -a 2 \
    -o ${2}
rm ${espfilename}
rm ${2}.part

