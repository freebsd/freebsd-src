#!/bin/sh
#
# Module: mkisoimages.sh
# Author: Jordan K Hubbard
# Date:   22 June 2001
#
#
# This script is used by release/Makefile to build the (optional) ISO images
# for a FreeBSD release.  It is considered architecture dependent since each
# platform has a slightly unique way of making bootable CDs.  This script
# is also allowed to generate any number of images since that is more of
# publishing decision than anything else.
#
# Usage:
#
# mkisoimages.sh [-b] image-label image-name base-bits-dir [extra-bits-dir]
#
# Where -b is passed if the ISO image should be made "bootable" by
# whatever standards this architecture supports (may be unsupported),
# image-label is the ISO image label, image-name is the filename of the
# resulting ISO image, base-bits-dir contains the image contents and
# extra-bits-dir, if provided, contains additional files to be merged
# into base-bits-dir as part of making the image.

set -e

scriptdir=$(dirname $(realpath $0))
. ${scriptdir}/../scripts/tools.subr
. ${scriptdir}/../../tools/boot/install-boot.sh

if [ "$1" = "-b" ]; then
	MAKEFSARG="$4"
else
	MAKEFSARG="$3"
fi

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

if [ "$1" = "-b" ]; then
	# This is highly x86-centric and will be used directly below.
	bootable="-o bootimage=i386;$BASEBITSDIR/boot/cdboot -o no-emul-boot"

	# Make EFI system partition.
	espfilename=$(mktemp /tmp/efiboot.XXXXXX)
	# ESP file size in KB.
	espsize="2048"
	if [ -f "${BASEBITSDIR}/boot/loader_ia32.efi" ]; then
		extra_args="${BASEBITSDIR}/boot/loader_ia32.efi bootia32"
	fi
	make_esp_file ${espfilename} ${espsize} ${BASEBITSDIR}/boot/loader.efi bootx64 ${extra_args}
	bootable="$bootable -o bootimage=i386;${espfilename} -o no-emul-boot -o platformid=efi"

	shift
else
	bootable=""
fi

if [ $# -lt 3 ]; then
	echo "Usage: $0 [-b] image-label image-name base-bits-dir [extra-bits-dir]"
	exit 1
fi

LABEL=`echo "$1" | tr '[:lower:]' '[:upper:]'`; shift
NAME="$1"; shift
# MAKEFSARG extracted already
shift

publisher="The FreeBSD Project.  https://www.FreeBSD.org/"
echo "/dev/iso9660/$LABEL / cd9660 ro 0 0" > "$BASEBITSDIR/etc/fstab"
if [ -n "${METALOG}" ]; then
	metalogfilename=$(mktemp /tmp/metalog.XXXXXX)
	cat ${METALOG} > ${metalogfilename}
	echo "./etc/fstab type=file uname=root gname=wheel mode=0644" >> ${metalogfilename}
	MAKEFSARG=${metalogfilename}
fi
$MAKEFS -D -N ${BASEBITSDIR}/etc -t cd9660 $bootable -o rockridge -o label="$LABEL" -o publisher="$publisher" "$NAME" "$MAKEFSARG" "$@"
rm -f "$BASEBITSDIR/etc/fstab"
rm -f ${espfilename}
if [ -n "${METALOG}" ]; then
	rm ${metalogfilename}
fi

if [ "$bootable" != "" ]; then
	# Look for the EFI System Partition image we dropped in the ISO image.
	for entry in `$ETDUMP --format shell $NAME`; do
		eval $entry
		if [ "$et_platform" = "efi" ]; then
			espstart=`expr $et_lba \* 2048`
			espsize=`expr $et_sectors \* 512`
			espparam="-p efi::$espsize:$espstart"
			break
		fi
	done

	# Create a GPT image containing the partitions we need for hybrid boot.
	hybridfilename=$(mktemp /tmp/hybrid.img.XXXXXX)
	if [ "$(uname -s)" = "Linux" ]; then
		imgsize=`stat -c %s "$NAME"`
	else
		imgsize=`stat -f %z "$NAME"`
	fi
	$MKIMG -s gpt \
	    --capacity $imgsize \
	    -b "$BASEBITSDIR/boot/pmbr" \
	    -p freebsd-boot:="$BASEBITSDIR/boot/isoboot" \
	    $espparam \
	    -o $hybridfilename

	# Drop the PMBR, GPT, and boot code into the System Area of the ISO.
	dd if=$hybridfilename of="$NAME" bs=32k count=1 conv=notrunc
	rm -f $hybridfilename
fi
