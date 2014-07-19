#!/bin/sh
#-
# Copyright (c) 2014 The FreeBSD Foundation
# All rights reserved.
#
# This software was developed by Glen Barber
# under sponsorship from the FreeBSD Foundation.
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
# $FreeBSD$
#

PATH=/bin:/usr/bin:/sbin:/usr/sbin:/usr/local/bin:/usr/local/sbin
export PATH

if [ $# -ne 2 ]; then
	echo "make-memstick.sh /path/to/directory /path/to/image/file"
	exit 1
fi

if [ -e ${2} ]; then
	echo "won't overwrite ${2}"
	exit 1
fi

main() {
	mkdir -p ${1}
	touch ${2}.raw
	truncate -s 3G ${2}.raw
	mddev=$(mdconfig -a -t vnode -f ${2}.raw)

	gpart create -s gpt /dev/${mddev}
	gpart add -t freebsd-boot -a 1m -s 512k -l bootfs /dev/${mddev}
	gpart add -t freebsd-ufs -a 1m -l rootfs /dev/${mddev}
	newfs -L rootfs /dev/${mddev}p2
	mount /dev/${mddev}p2 ${1}
	mkdir -p ${1}/dev

	for i in base kernel lib32; do
		tar -xzf ${i}.txz -C ${1}
	done

	gpart bootcode -b ${1}/boot/pmbr -p ${1}/boot/gptboot -i 1 /dev/${mddev}

	echo "# Custom /etc/fstab for Microsoft Azure VM images" \
		> ${1}/etc/fstab
	echo "/dev/gpt/rootfs	/	ufs	rw	2	2" \
		>> ${1}/etc/fstab

	cp /etc/resolv.conf ${1}/etc/resolv.conf
	mount -t devfs devfs ${1}/dev

	chroot ${1} /etc/rc.d/ldconfig forcestart
	chroot ${1} env ASSUME_ALWAYS_YES=1 /usr/sbin/pkg install -y \
		python python2 python27 py27-asn1 sudo bash
	fetch -o ${1}/usr/sbin/waagent http://people.freebsd.org/~gjb/waagent
	chmod +x ${1}/usr/sbin/waagent
	chroot ${1} ln -s /usr/local/bin/python /usr/bin/python
	rm -f ${1}/etc/resolv.conf

	chroot ${1} /usr/sbin/waagent -verbose -install
	cat ${1}/var/log/waagent.log
	yes | chroot ${1} /usr/sbin/waagent -deprovision

	echo 'sshd_enable="YES"' > ${1}/etc/rc.conf
	echo 'ifconfig_hn0="SYNCDHCP"' >> ${1}/etc/rc.conf
	echo 'waagent_enable="YES"' >> ${1}/etc/rc.conf

	# Make sure we wait until the md(4) is unmounted before destroying it.
	while ! umount ${1}/dev; do
		sleep 1
	done
	while ! umount ${1}; do
		sleep 1
	done

	/usr/bin/mkimg -f vhdf \
		-s gpt -b /boot/pmbr \
		-p freebsd-boot:=/dev/${mddev}p1 \
		-p freebsd-ufs/rootfs:=/dev/${mddev}p2 \
		-p freebsd-swap::1M \
		-p freebsd-ufs::1G \
		-o ${2}

	mdconfig -d -u ${mddev}
}

main "$@"
