#!/bin/sh
#
# $FreeBSD$
#

# This script is intended to be called by release/release.sh to build ARM
# images for release.  It is not intended to be run directly.  This sets up
# the software needed within a build chroot, then runs crochet to provide
# downloadable images for embedded devices.

set -e

before_build() {
	case ${KERNEL} in
		BEAGLEBONE)
			KNOWNHASH="4150e5a4480707c55a8d5b4570262e43af68d8ed3bdc0a433d8e7df47989a69e"
			chroot ${CHROOTDIR} fetch -o /tmp/crochet/u-boot-2013.04.tar.bz2 \
				http://people.freebsd.org/~gjb/u-boot-2013.04.tar.bz2
			UBOOT_HASH="$(sha256 -q ${CHROOTDIR}/tmp/crochet/u-boot-2013.04.tar.bz2)"
			if [ "${UBOOT_HASH}" != "${KNOWNHASH}" ]; then
				echo "Checksum mismatch!  Exiting now."
				exit 1
			fi
			chroot ${CHROOTDIR} tar xf /tmp/crochet/u-boot-2013.04.tar.bz2 \
				-C /tmp/crochet/ 
			;;
		*)
			# Fallthrough.
			;;
	esac
}

install_crochet() {
	chroot ${CHROOTDIR} svn co -q ${CROCHETSRC}/${CROCHETBRANCH} \
		/tmp/crochet
}

install_uboot() {
	# Only fetch u-boot sources if UBOOTSRC is set; otherwise it is
	# not needed.
	if [ "X${UBOOTSRC}" != "X" ]; then
		continue
	else
		return 0
	fi
	chroot ${CHROOTDIR} svn co -q ${UBOOTSRC}/${UBOOTBRANCH} \
		/${UBOOTDIR}
}

main() {
	# Build gcc for use in the chroot for arm builds.
	eval chroot ${CHROOTDIR} make -C /usr/src/gnu/usr.bin/cc \
		WITH_GCC=1 obj depend all install
	# Build the 'xdev' target for crochet.
	eval chroot ${CHROOTDIR} make -C /usr/src \
		XDEV=${XDEV} XDEV_ARCH=${XDEV_ARCH} WITH_GCC=1 xdev

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

	mkdir -p ${CHROOTDIR}/tmp/crochet/work
	before_build
	install_crochet
	install_uboot
	eval chroot ${CHROOTDIR} /bin/sh /tmp/crochet/crochet.sh \
		-c /tmp/external/${XDEV}/crochet-${KERNEL}.conf
	mkdir -p ${CHROOTDIR}/R/
	cp -p ${CHROOTDIR}/usr/obj/*.img ${CHROOTDIR}/R/
	cd ${CHROOTDIR}/R/ && sha256 FreeBSD*.img > CHECKSUM.SHA256
	cd ${CHROOTDIR}/R/ && md5 FreeBSD*.img > CHECKSUM.MD5
}

main "$@"
exit 0
