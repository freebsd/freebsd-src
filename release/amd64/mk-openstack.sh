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
# mk-openstack.sh: Create virtual machine disk images for Openstack
#
# $FreeBSD$
#

export PATH="/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin"

usage() {
	echo "Usage:"
	echo -n "$(basename ${0}) vm-openstack <base image>"
	echo " <source tree> <dest dir> <disk image size> <vm image name>"
	exit 1
}

panic() {
	msg="${@}"
	printf "${msg}\n"
	if [ ! -z "${mddev}" ]; then
		mdconfig -d -u ${mddev}
	fi
	# Do not allow one failure case to chain through any remaining image
	# builds.
	exit 0
}

vm_create_openstack() {
	# Arguments:
	# vm-openstack <base image> <source tree> <dest dir> <disk image size>
	#	<vm image name>

	VMBASE="${1}"
	WORLDDIR="${2}"
	DESTDIR="${3}"
	VMSIZE="${4}"
	VMIMAGE="${5}"

	if [ -z "${VMBASE}" -o -z "${WORLDDIR}" -o -z "${DESTDIR}" \
		-o -z "${VMSIZE}" -o -z "${VMIMAGE}" ]; then
			usage
	fi

	trap "umount ${DESTDIR}/dev ${DESTDIR}" INT QUIT TRAP ABRT TERM

	i=0
	mkdir -p ${DESTDIR}
	truncate -s ${VMSIZE} ${VMBASE}
	mddev=$(mdconfig -f ${VMBASE})
	newfs -j /dev/${mddev}
	mkdir -p ${DESTDIR}
	mount /dev/${mddev} ${DESTDIR}
	make -C ${WORLDDIR} DESTDIR=$(realpath ${DESTDIR}) \
		installworld installkernel distribution || \
		panic "\n\nCannot install the base system to ${DESTDIR}."
	mount -t devfs devfs ${DESTDIR}/dev
	chroot ${DESTDIR} /usr/bin/newaliases
	echo '# Custom /etc/fstab for FreeBSD VM images' \
		> ${DESTDIR}/etc/fstab
	echo '/dev/gpt/rootfs	/	ufs	rw	2	2' \
		>> ${DESTDIR}/etc/fstab
	echo '/dev/gpt/swapfs	none	swap	sw	0	0' \
		>> ${DESTDIR}/etc/fstab

	chroot ${DESTDIR} /etc/rc.d/ldconfig forcestart
	chroot ${DESTDIR} env ASSUME_ALWAYS_YES=yes /usr/sbin/pkg bootstrap -y
	if [ ! -z "${VM_EXTRA_PACKAGES}" ]; then
	        chroot ${DESTDIR} env ASSUME_ALWAYS_YES=yes /usr/sbin/pkg install -y \
	                ${VM_EXTRA_PACKAGES}
	fi

	rm -f ${DESTDIR}/etc/resolv.conf
	echo 'sshd_enable="YES"' > ${DESTDIR}/etc/rc.conf
	echo 'ifconfig_DEFAULT="SYNCDHCP"' >> ${DESTDIR}/etc/rc.conf

	if [ ! -z "${VM_RC_LIST}" ]; then
		for _rcvar in ${VM_RC_LIST}; do
			echo ${_rcvar}_enable="YES" >> ${DESTDIR}/etc/rc.conf
		done
	fi

	sync

	while ! umount ${DESTDIR}/dev ${DESTDIR}; do
		i=$(( $i + 1 ))
		if [ $i -ge 10 ]; then
			# This should never happen.  But, it has happened.
			msg="Cannot umount(8) ${DESTDIR}\n"
			msg="${msg}Something has gone horribly wrong."
			panic "${msg}"
		fi
		sleep 1
	done

	echo "Creating image...  Please wait."

	mkimg -f ${OPENSTACK_FORMAT} -s gpt \
		-b /boot/pmbr -p freebsd-boot/bootfs:=/boot/gptboot \
		-p freebsd-swap/swapfs::1G \
		-p freebsd-ufs/rootfs:=${VMBASE} \
		-o ${VMIMAGE}

	return 0
}

main() {
	cmd="${1}"
	shift 1

	if [ -e "${OPENSTACKCONF}" -a ! -c "${OPENSTACKCONF}" ]; then
		. ${OPENSTACKCONF}
	fi

	case ${cmd} in
		vm-openstack)
			eval vm_create_openstack "$@" || return 0
			;;
		*|\?)
			usage
			;;
	esac

	return 0
}

main "$@"
