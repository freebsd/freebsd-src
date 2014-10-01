#!/bin/sh
#-
# Copyright (c) 2014 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Glen Barber under sponsorship
# from the FreeBSD Foundation.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# mk-vmimage.sh: Create virtual machine disk images in various formats.
#
# $FreeBSD$
#

PATH="/bin:/usr/bin:/sbin:/usr/sbin"
export PATH

usage_vm_base() {
	echo -n "$(basename ${0}) vm-base <base image> <source tree>"
	echo	" <dest dir> <disk image size>"
	return 0
}

usage_vm_image() {
	echo -n "$(basename ${0}) vm-image <base image> <image format>"
	echo	" <output image>"
	return 0
}

usage() {
	echo "Usage:"
	echo "$(basename ${0}) [vm-base|vm-image] [...]"
	echo
	usage_vm_base
	echo
	usage_vm_image
	exit 1
}

panic() {
	rc="${1}"
	shift 1
	msg="${@}"
	printf "${msg}\n"
	if [ ! -z "${mddev}" ]; then
		mdconfig -d -u ${mddev}
	fi
	# Do not allow one failure case to chain through any remaining image
	# builds.
	exit 0
}

vm_create_baseimage() {
	# Creates the UFS root filesystem for the virtual machine disk,
	# written to the formatted disk image with mkimg(1).
	#
	# Arguments:
	# vm-base <base image> <source tree> <dest dir> <disk image size>

	VMBASE="${1}"
	WORLDDIR="${2}"
	DESTDIR="${3}"
	VMSIZE="${4}"

	if [ -z "${VMBASE}" -o -z "${WORLDDIR}" -o -z "${DESTDIR}" \
		-o -z "${VMSIZE}" ]; then
			usage
	fi

	i=0
	mkdir -p ${DESTDIR}
	truncate -s ${VMSIZE} ${VMBASE}
	mddev=$(mdconfig -f ${VMBASE})
	newfs -j /dev/${mddev}
	mount /dev/${mddev} ${DESTDIR}
	cd ${WORLDDIR} && \
		make DESTDIR=${DESTDIR} \
		installworld installkernel distribution || \
		panic 1 "\n\nCannot install the base system to ${DESTDIR}."
	chroot ${DESTDIR} /usr/bin/newaliases
	echo '# Custom /etc/fstab for FreeBSD VM images' \
		> ${DESTDIR}/etc/fstab
	echo '/dev/gpt/rootfs	/	ufs	rw	2	2' \
		>> ${DESTDIR}/etc/fstab
	echo '/dev/gpt/swapfs	none	swap	sw	0	0' \
		>> ${DESTDIR}/etc/fstab
	sync
	while ! umount ${DESTDIR}; do
		i=$(( $i + 1 ))
		if [ $i -ge 10 ]; then
			# This should never happen.  But, it has happened.
			msg="Cannot umount(8) ${DESTDIR}\n"
			msg="${msg}Something has gone horribly wrong."
			panic 1 "${msg}"
		fi
		sleep 1
	done

	return 0
}

vm_create_vmdisk() {
	# Creates the virtual machine disk image from the raw disk image.
	#
	# Arguments:
	# vm-image <base image> <image format> <output image>"

	VMBASE="${1}"
	FORMAT="${2}"
	VMIMAGE="${3}"

	if [ -z "${VMBASE}" -o -z "${FORMAT}" -o -z "${VMIMAGE}" ]; then
		usage
	fi

	mkimg_version=$(mkimg --version 2>/dev/null | awk '{print $2}')

	# We need mkimg(1) '--version' output, at minimum, to be able to
	# tell what virtual machine disk image formats are available.
	# Bail if mkimg(1) reports an empty '--version' value.
	if [ -z "${mkimg_version}" ]; then
		msg="Cannot determine mkimg(1) version.\n"
		msg="${msg}Cannot continue without a known mkimg(1) version."
		panic 0 "${msg}"
	fi

	if ! mkimg --formats 2>/dev/null | grep -q ${FORMAT}; then
		panic 0 "'${FORMAT}' is not supported by this mkimg(1).\n"
	fi

	case ${FORMAT} in
		vhd)
			mkimg_format=vhdf
			;;
		*)
			mkimg_format=${FORMAT}
			;;
	esac

	mkimg -f ${mkimg_format} -s gpt \
		-b /boot/pmbr -p freebsd-boot/bootfs:=/boot/gptboot \
		-p freebsd-swap/swapfs::1G \
		-p freebsd-ufs/rootfs:=${VMBASE} \
		-o ${VMIMAGE}

	return 0
}

main() {
	cmd="${1}"
	shift 1

	case ${cmd} in
		vm-base)
			eval vm_create_baseimage "$@" || return 0
			;;
		vm-image)
			eval vm_create_vmdisk "$@" || return 0
			;;
		*|\?)
			usage
			;;
	esac

	return 0
}

main "$@"
