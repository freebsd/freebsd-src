#!/bin/sh
#
# $FreeBSD$
#

# This script is intended to be called by release/release.sh to build ARM
# images for release.  It is not intended to be run directly.  This sets up
# the software needed within a build chroot, then runs crochet to provide
# downloadable images for embedded devices.

set -e

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

	install_crochet
	install_uboot
	mkdir -p ${CHROOTDIR}/tmp/crochet/work
	eval chroot ${CHROOTDIR} /bin/sh /tmp/crochet/crochet.sh \
		-c /usr/src/tools/release/${XDEV}/crochet-${KERNEL}.conf
}

main "$@"
exit 0
