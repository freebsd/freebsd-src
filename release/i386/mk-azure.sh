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
# mk-azure.sh: Create virtual machine disk images for Microsoft Azure
#
# $FreeBSD$
#

export PATH="/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin"

usage() {
	echo "Usage:"
	echo -n "$(basename ${0}) vm-azure <base image>"
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

vm_create_azure() {
	# Arguments:
	# vm-azure <base image> <source tree> <dest dir> <disk image size> <vm image name>

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
		panic 1 "\n\nCannot install the base system to ${DESTDIR}."
	mount -t devfs devfs ${DESTDIR}/dev
	chroot ${DESTDIR} /usr/bin/newaliases
	echo '# Custom /etc/fstab for FreeBSD VM images' \
		> ${DESTDIR}/etc/fstab
	echo '/dev/gpt/rootfs	/	ufs	rw	2	2' \
		>> ${DESTDIR}/etc/fstab
	# Although a swap partition is created, it is not used in Azure.
	echo '#/dev/gpt/swapfs	none	swap	sw	0	0' \
		>> ${DESTDIR}/etc/fstab

	chroot ${DESTDIR} /etc/rc.d/ldconfig forcestart
	chroot ${DESTDIR} env ASSUME_ALWAYS_YES=yes /usr/sbin/pkg bootstrap -y
	chroot ${DESTDIR} env ASSUME_ALWAYS_YES=yes /usr/sbin/pkg install -y \
	        python python2 python27 py27-asn1 sudo bash
	if [ ! -z "${VM_EXTRA_PACKAGES}" ]; then
	        chroot ${DESTDIR} env ASSUME_ALWAYS_YES=yes /usr/sbin/pkg install -y \
	                ${VM_EXTRA_PACKAGES}
	fi

	fetch -o ${DESTDIR}/usr/sbin/waagent \
		http://people.freebsd.org/~gjb/waagent
	chmod +x ${DESTDIR}/usr/sbin/waagent
	rm -f ${DESTDIR}/etc/resolv.conf
	chroot ${DESTDIR} /usr/sbin/waagent -verbose -install
	yes | chroot ${DESTDIR} /usr/sbin/waagent -deprovision
	echo 'sshd_enable="YES"' > ${DESTDIR}/etc/rc.conf
	echo 'ifconfig_hn0="SYNCDHCP"' >> ${DESTDIR}/etc/rc.conf
	echo 'waagent_enable="YES"' >> ${DESTDIR}/etc/rc.conf

	echo 'console="comconsole vidconsole"' >> ${DESTDIR}/boot/loader.conf
	echo 'comconsole_speed="115200"' >> ${DESTDIR}/boot/loader.conf

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
			panic 1 "${msg}"
		fi
		sleep 1
	done

	echo "Creating image...  Please wait."

	mkimg -f vhdf -s gpt \
		-b /boot/pmbr -p freebsd-boot/bootfs:=/boot/gptboot \
		-p freebsd-swap/swapfs::1G \
		-p freebsd-ufs/rootfs:=${VMBASE} \
		-o ${VMIMAGE}.raw

	if [ ! -x "/usr/local/bin/qemu-img" ]; then
		env ASSUME_ALWAYS_YES=yes pkg install -y emulators/qemu-devel
	fi

	size=$(qemu-img info -f raw --output json ${VMIMAGE}.raw | awk '/virtual-size/ {print $2}' | tr -d ',')
	size=$(( ( ${size} / ( 1024 * 1024 ) + 1 ) * ( 1024 * 1024 ) ))
	qemu-img resize ${VMIMAGE}.raw ${size}
	qemu-img convert -f raw -o subformat=fixed -O vpc ${VMIMAGE}.raw ${VMIMAGE}

	return 0
}

main() {
	cmd="${1}"
	shift 1

	if [ -e "${AZURECONF}" -a ! -c "${AZURECONF}" ]; then
		. ${AZURECONF}
	fi

	case ${cmd} in
		vm-azure)
			eval vm_create_azure "$@" || return 0
			;;
		*|\?)
			usage
			;;
	esac

	return 0
}

main "$@"
