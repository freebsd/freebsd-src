#!/bin/sh
#-
# Copyright (c) 2013, 2014 The FreeBSD Foundation
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

# This script is intended to be called by release/release.sh to build ARM
# images for release.  It is not intended to be run directly.  This sets up
# the software needed within a build chroot, then runs crochet to provide
# downloadable images for embedded devices.

set -e

before_build() {
	WANT_UBOOT=
	KNOWNHASH=
	UBOOT_VERSION=
	case ${KERNEL} in
		BEAGLEBONE)
			WANT_UBOOT=1
			KNOWNHASH="4150e5a4480707c55a8d5b4570262e43af68d8ed3bdc0a433d8e7df47989a69e"
			UBOOT_VERSION="u-boot-2013.04"
			;;
		PANDABOARD)
			WANT_UBOOT=1
			KNOWNHASH="e08e20a6979bfca6eebb9a2b0e42aa4416af3d796332fd63a3470495a089d496"
			UBOOT_VERSION="u-boot-2012.07"
			;;
		WANDBOARD-QUAD)
			WANT_UBOOT=1
			KNOWNHASH="0d71e62beb952b41ebafb20a7ee4df2f960db64c31b054721ceb79ff14014c55"
			UBOOT_VERSION="u-boot-2013.10"
			;;
		*)
			# Fallthrough.
			;;
	esac
	if [ ! -z ${WANT_UBOOT} ]; then
		chroot ${CHROOTDIR} fetch -o /tmp/crochet/${UBOOT_VERSION}.tar.bz2 \
			http://people.freebsd.org/~gjb/${UBOOT_VERSION}.tar.bz2
		UBOOT_HASH="$(sha256 -q ${CHROOTDIR}/tmp/crochet/${UBOOT_VERSION}.tar.bz2)"
		if [ "${UBOOT_HASH}" != "${KNOWNHASH}" ]; then
			echo "Checksum mismatch!  Exiting now."
			exit 1
		fi
		chroot ${CHROOTDIR} tar xf /tmp/crochet/${UBOOT_VERSION}.tar.bz2 \
			-C /tmp/crochet/ 
	fi
}

install_crochet() {
	chroot ${CHROOTDIR} svn co -q ${CROCHETSRC}/${CROCHETBRANCH} \
		/tmp/crochet
}

install_uboot() {
	# Only fetch u-boot sources if UBOOTSRC is set; otherwise it is
	# not needed.
	if [ -n "${UBOOTSRC}" ]; then
		continue
	else
		return 0
	fi
	chroot ${CHROOTDIR} svn co -q ${UBOOTSRC}/${UBOOTBRANCH} \
		/${UBOOTDIR}
}

main() {
	# Build the 'xdev' target for crochet.
	eval chroot ${CHROOTDIR} make -C /usr/src \
		${XDEV_FLAGS} TARGET=${XDEV} TARGET_ARCH=${XDEV_ARCH} \
		${WORLD_FLAGS} xdev

	# Run the ldconfig(8) startup script so /var/run/ld-elf*.so.hints
	# is created.
	eval chroot ${CHROOTDIR} /etc/rc.d/ldconfig forcerestart
	# Install security/ca_root_nss since we need to check the https
	# certificate of github.
	eval chroot ${CHROOTDIR} make -C /usr/ports/security/ca_root_nss \
		OPTIONS_SET="ETCSYMLINK" BATCH=1 FORCE_PKG_REGISTER=1 \
		install clean distclean
	EMBEDDEDPORTS="${EMBEDDEDPORTS} devel/subversion"
	for _PORT in ${EMBEDDEDPORTS}; do
		eval chroot ${CHROOTDIR} make -C /usr/ports/${_PORT} \
			BATCH=1 FORCE_PKG_REGISTER=1 install clean distclean
	done

	eval chroot ${CHROOTDIR} make -C /usr/src/gnu/usr.bin/cc \
		WITH_GCC=1 ${WORLD_FLAGS} -j1 obj depend all install

	mkdir -p ${CHROOTDIR}/tmp/crochet/work
	before_build
	install_crochet
	install_uboot
	eval chroot ${CHROOTDIR} /bin/sh /tmp/crochet/crochet.sh \
		-c /tmp/external/${XDEV}/crochet-${KERNEL}.conf
	mkdir -p ${CHROOTDIR}/R/
	cp -p ${CHROOTDIR}/usr/obj/*.img ${CHROOTDIR}/R/
	bzip2 ${CHROOTDIR}/R/FreeBSD*.img
	cd ${CHROOTDIR}/R/ && sha256 FreeBSD*.img.bz2 > CHECKSUM.SHA256
	cd ${CHROOTDIR}/R/ && md5 FreeBSD*.img.bz2 > CHECKSUM.MD5
}

main "$@"
exit 0
