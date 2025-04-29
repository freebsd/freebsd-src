#!/bin/sh
#-
# Copyright (c) 2020-2021 Rubicon Communications, LLC (netgate.com)
# Copyright (c) 2013-2019 The FreeBSD Foundation
# Copyright (c) 2013 Glen Barber
# Copyright (c) 2011 Nathan Whitehorn
# All rights reserved.
#
# Portions of this software were developed by Glen Barber
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
# release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
# Based on release/generate-release.sh written by Nathan Whitehorn
#

export PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin"

VERSION=3

# Prototypes that can be redefined per-chroot or per-target.
load_chroot_env() { }
load_target_env() { }
buildenv_setup() { }

usage() {
	echo "Usage: $0 [-c release.conf]"
	exit 1
}

# env_setup(): Set up the default build environment variables, such as the
# CHROOTDIR, VCSCMD, GITROOT, etc.  This is called before the release.conf
# file is sourced, if '-c <release.conf>' is specified.
env_setup() {
	# The directory within which the release will be built.
	CHROOTDIR="/scratch"
	if [ -z "${RELENGDIR}" ]; then
		export RELENGDIR="$(dirname $(realpath ${0}))"
	fi

	# The default version control system command to obtain the sources.
	for _dir in /usr/bin /usr/local/bin; do
		[ -x "${_dir}/git" ] && VCSCMD="/${_dir}/git"
		[ ! -z "${VCSCMD}" ] && break 2
	done

	if [ -z "${VCSCMD}" -a -z "${NOGIT}" ]; then
		echo "*** The devel/git port/package is required."
		exit 1
	fi
	VCSCMD="/usr/local/bin/git clone -q"

	# The default git checkout server, and branches for src/, doc/,
	# and ports/.
	GITROOT="https://git.FreeBSD.org/"
	SRCBRANCH="main"
	PORTBRANCH="main"
	GITSRC="src.git"
	GITPORTS="ports.git"

	# Set for embedded device builds.
	EMBEDDEDBUILD=

	# The default make.conf and src.conf to use.  Set to /dev/null
	# by default to avoid polluting the chroot(8) environment with
	# non-default settings.
	MAKE_CONF="/dev/null"
	SRC_CONF="/dev/null"

	# The number of make(1) jobs, defaults to the number of CPUs available
	# for buildworld, and half of number of CPUs available for buildkernel
	# and 'make release'.
	WORLD_FLAGS="-j$(sysctl -n hw.ncpu)"
	KERNEL_FLAGS="-j$(( $(( $(sysctl -n hw.ncpu) + 1 )) / 2))"
	RELEASE_FLAGS="-j$(( $(( $(sysctl -n hw.ncpu) + 1 )) / 2))"

	MAKE_FLAGS="-s"

	# The name of the kernel to build, defaults to GENERIC.
	KERNEL="GENERIC"

	# Set to non-empty value to disable checkout of doc/ and/or ports/.
	NOPORTS=

	# Set to non-empty value to disable distributing source tree.
	NOSRC=

	# Set to non-empty value to build dvd1.iso as part of the release.
	WITH_DVD=
	WITH_COMPRESSED_IMAGES=

	# Set to non-empty value to build virtual machine images as part of
	# the release.
	WITH_VMIMAGES=
	WITH_COMPRESSED_VMIMAGES=
	XZ_THREADS=0

	# Set to non-empty value to build virtual machine images for various
	# cloud providers as part of the release.
	WITH_CLOUDWARE=

	# Set to non-empty to build OCI images as part of the release
	WITH_OCIIMAGES=

	return 0
} # env_setup()

# env_check(): Perform sanity tests on the build environment, such as ensuring
# files/directories exist, as well as adding backwards-compatibility hacks if
# necessary.  This is called unconditionally, and overrides the defaults set
# in env_setup() if '-c <release.conf>' is specified.
env_check() {
	chroot_build_release_cmd="chroot_build_release"

	# Prefix the branches with the GITROOT for the full checkout URL.
	SRC="${GITROOT}${GITSRC}"
	PORT="${GITROOT}${GITPORTS}"

	if [ -n "${EMBEDDEDBUILD}" ]; then
		WITH_DVD=
		WITH_COMPRESSED_IMAGES=
		case ${EMBEDDED_TARGET}:${EMBEDDED_TARGET_ARCH} in
			arm:arm*|arm64:aarch64|riscv:riscv64*)
				chroot_build_release_cmd="chroot_arm_build_release"
				;;
			*)
				;;
		esac
	fi

	# If NOSRC and/or NOPORTS are unset, they must not pass to make
	# as variables.  The release makefile verifies definedness of the
	# NOPORTS variable instead of its value.
	SRCPORTS=
	if [ -n "${NOPORTS}" ]; then
		SRCPORTS="NOPORTS=yes"
	fi
	if [ -n "${NOSRC}" ]; then
		SRCPORTS="${SRCPORTS}${SRCPORTS:+ }NOSRC=yes"
	fi

	# The aggregated build-time flags based upon variables defined within
	# this file, unless overridden by release.conf.  In most cases, these
	# will not need to be changed.
	CONF_FILES="__MAKE_CONF=${MAKE_CONF} SRCCONF=${SRC_CONF}"
	NOCONF_FILES="__MAKE_CONF=/dev/null SRCCONF=/dev/null"
	if [ -n "${TARGET}" ] && [ -n "${TARGET_ARCH}" ]; then
		ARCH_FLAGS="TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH}"
	else
		ARCH_FLAGS=
	fi

	if [ -z "${CHROOTDIR}" ]; then
		echo "Please set CHROOTDIR."
		exit 1
	fi

	if [ $(id -u) -ne 0 ]; then
		echo "Needs to be run as root."
		exit 1
	fi

	# Unset CHROOTBUILD_SKIP if the chroot(8) does not appear to exist.
	if [ ! -z "${CHROOTBUILD_SKIP}" -a ! -e ${CHROOTDIR}/bin/sh ]; then
		CHROOTBUILD_SKIP=
	fi

	CHROOT_MAKEENV="${CHROOT_MAKEENV} \
		MAKEOBJDIRPREFIX=${CHROOTDIR}/tmp/obj"
	CHROOT_WMAKEFLAGS="${MAKE_FLAGS} ${WORLD_FLAGS} ${NOCONF_FILES}"
	CHROOT_IMAKEFLAGS="${WORLD_FLAGS} ${NOCONF_FILES}"
	CHROOT_DMAKEFLAGS="${WORLD_FLAGS} ${NOCONF_FILES}"
	RELEASE_WMAKEFLAGS="${MAKE_FLAGS} ${WORLD_FLAGS} ${ARCH_FLAGS} \
		${CONF_FILES}"
	RELEASE_KMAKEFLAGS="${MAKE_FLAGS} ${KERNEL_FLAGS} \
		KERNCONF=\"${KERNEL}\" ${ARCH_FLAGS} ${CONF_FILES}"
	RELEASE_RMAKEFLAGS="${ARCH_FLAGS} ${RELEASE_FLAGS} \
		KERNCONF=\"${KERNEL}\" ${CONF_FILES} ${SRCPORTS} \
		WITH_DVD=${WITH_DVD} WITH_VMIMAGES=${WITH_VMIMAGES} \
		WITH_CLOUDWARE=${WITH_CLOUDWARE} WITH_OCIIMAGES=${WITH_OCIIMAGES} \
		XZ_THREADS=${XZ_THREADS}"

	return 0
} # env_check()

# chroot_setup(): Prepare the build chroot environment for the release build.
chroot_setup() {
	load_chroot_env
	mkdir -p ${CHROOTDIR}/usr

	if [ -z "${SRC_UPDATE_SKIP}" ]; then
		if [ -d "${CHROOTDIR}/usr/src/.git" ]; then
			git -C ${CHROOTDIR}/usr/src pull -q
		else
			${VCSCMD} ${SRC} -b ${SRCBRANCH} ${CHROOTDIR}/usr/src
		fi
	fi
	if [ -z "${NOPORTS}" ] && [ -z "${PORTS_UPDATE_SKIP}" ]; then
		if [ -d "${CHROOTDIR}/usr/ports/.git" ]; then
			git -C ${CHROOTDIR}/usr/ports pull -q
		else
			${VCSCMD} ${PORT} -b ${PORTBRANCH} ${CHROOTDIR}/usr/ports
		fi
	fi

	if [ -z "${CHROOTBUILD_SKIP}" ]; then
		cd ${CHROOTDIR}/usr/src
		env ${CHROOT_MAKEENV} make ${CHROOT_WMAKEFLAGS} buildworld
		env ${CHROOT_MAKEENV} make ${CHROOT_IMAKEFLAGS} installworld \
			DESTDIR=${CHROOTDIR}
		env ${CHROOT_MAKEENV} make ${CHROOT_DMAKEFLAGS} distribution \
			DESTDIR=${CHROOTDIR}
	fi

	return 0
} # chroot_setup()

# extra_chroot_setup(): Prepare anything additional within the build
# necessary for the release build.
extra_chroot_setup() {
	mkdir -p ${CHROOTDIR}/dev
	mount -t devfs devfs ${CHROOTDIR}/dev
	[ -e /etc/resolv.conf -a ! -e ${CHROOTDIR}/etc/resolv.conf ] && \
		cp /etc/resolv.conf ${CHROOTDIR}/etc/resolv.conf
	# Run ldconfig(8) in the chroot directory so /var/run/ld-elf*.so.hints
	# is created.  This is needed by ports-mgmt/pkg.
	eval chroot ${CHROOTDIR} /etc/rc.d/ldconfig forcerestart

	# If MAKE_CONF and/or SRC_CONF are set and not character devices
	# (/dev/null), copy them to the chroot.
	if [ -e ${MAKE_CONF} ] && [ ! -c ${MAKE_CONF} ]; then
		mkdir -p ${CHROOTDIR}/$(dirname ${MAKE_CONF})
		cp ${MAKE_CONF} ${CHROOTDIR}/${MAKE_CONF}
	fi
	if [ -e ${SRC_CONF} ] && [ ! -c ${SRC_CONF} ]; then
		mkdir -p ${CHROOTDIR}/$(dirname ${SRC_CONF})
		cp ${SRC_CONF} ${CHROOTDIR}/${SRC_CONF}
	fi

	_gitcmd="$(which git)"
	if [ -z "${NOGIT}" -a -z "${_gitcmd}" ]; then
		# Install git from ports if the ports tree is available;
		# otherwise install the pkg.
		if [ -d ${CHROOTDIR}/usr/ports ]; then
			# Trick the ports 'run-autotools-fixup' target to do the right
			# thing.
			_OSVERSION=$(chroot ${CHROOTDIR} /usr/bin/uname -U)
			REVISION=$(chroot ${CHROOTDIR} make -C /usr/src/release -V REVISION)
			BRANCH=$(chroot ${CHROOTDIR} make -C /usr/src/release -V BRANCH)
			UNAME_r=${REVISION}-${BRANCH}
			GITUNSETOPTS="CONTRIB CURL CVS GITWEB GUI HTMLDOCS"
			GITUNSETOPTS="${GITUNSETOPTS} ICONV NLS P4 PERL"
			GITUNSETOPTS="${GITUNSETOPTS} SEND_EMAIL SUBTREE SVN"
			GITUNSETOPTS="${GITUNSETOPTS} PCRE PCRE2"
			PBUILD_FLAGS="OSVERSION=${_OSVERSION} BATCH=yes"
			PBUILD_FLAGS="${PBUILD_FLAGS} UNAME_r=${UNAME_r}"
			PBUILD_FLAGS="${PBUILD_FLAGS} OSREL=${REVISION}"
			PBUILD_FLAGS="${PBUILD_FLAGS} WRKDIRPREFIX=/tmp/ports"
			PBUILD_FLAGS="${PBUILD_FLAGS} DISTDIR=/tmp/distfiles"
			eval chroot ${CHROOTDIR} env OPTIONS_UNSET=\"${GITUNSETOPTS}\" \
				${PBUILD_FLAGS} \
				make -C /usr/ports/devel/git FORCE_PKG_REGISTER=1 \
				WRKDIRPREFIX=/tmp/ports \
				DISTDIR=/tmp/distfiles \
				install clean distclean
		else
			eval chroot ${CHROOTDIR} env ASSUME_ALWAYS_YES=yes \
				pkg install -y devel/git
			eval chroot ${CHROOTDIR} env ASSUME_ALWAYS_YES=yes \
				pkg clean -y
		fi
	fi

	if [ ! -z "${EMBEDDEDPORTS}" ]; then
		_OSVERSION=$(chroot ${CHROOTDIR} /usr/bin/uname -U)
		REVISION=$(chroot ${CHROOTDIR} make -C /usr/src/release -V REVISION)
		BRANCH=$(chroot ${CHROOTDIR} make -C /usr/src/release -V BRANCH)
		UNAME_r=${REVISION}-${BRANCH}
		PBUILD_FLAGS="OSVERSION=${_OSVERSION} BATCH=yes"
		PBUILD_FLAGS="${PBUILD_FLAGS} UNAME_r=${UNAME_r}"
		PBUILD_FLAGS="${PBUILD_FLAGS} OSREL=${REVISION}"
		PBUILD_FLAGS="${PBUILD_FLAGS} WRKDIRPREFIX=/tmp/ports"
		PBUILD_FLAGS="${PBUILD_FLAGS} DISTDIR=/tmp/distfiles"
		for _PORT in ${EMBEDDEDPORTS}; do
			eval chroot ${CHROOTDIR} env ${PBUILD_FLAGS} make -C \
				/usr/ports/${_PORT} \
				FORCE_PKG_REGISTER=1 deinstall install clean distclean
		done
	fi

	buildenv_setup

	return 0
} # extra_chroot_setup()

# chroot_build_target(): Build the userland and kernel for the build target.
chroot_build_target() {
	load_target_env
	if [ ! -z "${EMBEDDEDBUILD}" ]; then
		RELEASE_WMAKEFLAGS="${RELEASE_WMAKEFLAGS} \
			TARGET=${EMBEDDED_TARGET} \
			TARGET_ARCH=${EMBEDDED_TARGET_ARCH}"
		RELEASE_KMAKEFLAGS="${RELEASE_KMAKEFLAGS} \
			TARGET=${EMBEDDED_TARGET} \
			TARGET_ARCH=${EMBEDDED_TARGET_ARCH}"
	fi
	eval chroot ${CHROOTDIR} make -C /usr/src ${RELEASE_WMAKEFLAGS} buildworld
	eval chroot ${CHROOTDIR} make -C /usr/src ${RELEASE_KMAKEFLAGS} buildkernel
	if [ ! -z "${WITH_OCIIMAGES}" ]; then
		eval chroot ${CHROOTDIR} make -C /usr/src ${RELEASE_WMAKEFLAGS} packages
	fi

	return 0
} # chroot_build_target

# chroot_build_release(): Invoke the 'make release' target.
chroot_build_release() {
	load_target_env
	if [ ! -z "${WITH_VMIMAGES}" ]; then
		if [ -z "${VMFORMATS}" ]; then
			VMFORMATS="$(eval chroot ${CHROOTDIR} \
				make -C /usr/src/release -V VMFORMATS)"
		fi
		if [ -z "${VMSIZE}" ]; then
			VMSIZE="$(eval chroot ${CHROOTDIR} \
				make -C /usr/src/release ${ARCH_FLAGS} -V VMSIZE)"
		fi
		RELEASE_RMAKEFLAGS="${RELEASE_RMAKEFLAGS} \
			VMFORMATS=\"${VMFORMATS}\" VMSIZE=${VMSIZE}"
	fi
	eval chroot ${CHROOTDIR} make -C /usr/src/release \
		${RELEASE_RMAKEFLAGS} release
	eval chroot ${CHROOTDIR} make -C /usr/src/release \
		${RELEASE_RMAKEFLAGS} install DESTDIR=/R \
		WITH_COMPRESSED_IMAGES=${WITH_COMPRESSED_IMAGES} \
		WITH_COMPRESSED_VMIMAGES=${WITH_COMPRESSED_VMIMAGES}

	return 0
} # chroot_build_release()

efi_boot_name()
{
	case $1 in
		arm)
			echo "bootarm.efi"
			;;
		arm64)
			echo "bootaa64.efi"
			;;
		amd64)
			echo "bootx64.efi"
			;;
		riscv)
			echo "bootriscv64.efi"
			;;
	esac
}

# chroot_arm_build_release(): Create arm SD card image.
chroot_arm_build_release() {
	load_target_env
	case ${EMBEDDED_TARGET} in
		arm|arm64|riscv)
			if [ -e "${RELENGDIR}/tools/arm.subr" ]; then
				. "${RELENGDIR}/tools/arm.subr"
			fi
			;;
		*)
			;;
	esac
	[ ! -z "${RELEASECONF}" ] && . "${RELEASECONF}"
	export MAKE_FLAGS="${MAKE_FLAGS} TARGET=${EMBEDDED_TARGET}"
	export MAKE_FLAGS="${MAKE_FLAGS} TARGET_ARCH=${EMBEDDED_TARGET_ARCH}"
	export MAKE_FLAGS="${MAKE_FLAGS} ${CONF_FILES}"
	eval chroot ${CHROOTDIR} env WITH_UNIFIED_OBJDIR=1 make ${MAKE_FLAGS} -C /usr/src/release obj
	export WORLDDIR="$(eval chroot ${CHROOTDIR} make ${MAKE_FLAGS} -C /usr/src/release -V WORLDDIR)"
	export OBJDIR="$(eval chroot ${CHROOTDIR} env WITH_UNIFIED_OBJDIR=1 make ${MAKE_FLAGS} -C /usr/src/release -V .OBJDIR)"
	export DESTDIR="${OBJDIR}/${KERNEL}"
	export IMGBASE="${CHROOTDIR}/${OBJDIR}/${BOARDNAME}.img"
	export OSRELEASE="$(eval chroot ${CHROOTDIR} make ${MAKE_FLAGS} -C /usr/src/release \
		TARGET=${EMBEDDED_TARGET} TARGET_ARCH=${EMBEDDED_TARGET_ARCH} \
		-V OSRELEASE)"
	chroot ${CHROOTDIR} mkdir -p ${DESTDIR}
	chroot ${CHROOTDIR} truncate -s ${IMAGE_SIZE} ${IMGBASE##${CHROOTDIR}}
	export mddev=$(chroot ${CHROOTDIR} \
		mdconfig -f ${IMGBASE##${CHROOTDIR}} ${MD_ARGS})
	arm_create_disk
	arm_install_base
	arm_install_boot
	arm_install_uboot
	mdconfig -d -u ${mddev}
	chroot ${CHROOTDIR} rmdir ${DESTDIR}
	mv ${IMGBASE} ${CHROOTDIR}/${OBJDIR}/${OSRELEASE}-${BOARDNAME}.img
	chroot ${CHROOTDIR} mkdir -p /R
	chroot ${CHROOTDIR} cp -p ${OBJDIR}/${OSRELEASE}-${BOARDNAME}.img \
		/R/${OSRELEASE}-${BOARDNAME}.img
	chroot ${CHROOTDIR} xz -T ${XZ_THREADS} /R/${OSRELEASE}-${BOARDNAME}.img
	cd ${CHROOTDIR}/R && sha512 ${OSRELEASE}* \
		> CHECKSUM.SHA512
	cd ${CHROOTDIR}/R && sha256 ${OSRELEASE}* \
		> CHECKSUM.SHA256

	return 0
} # chroot_arm_build_release()

# main(): Start here.
main() {
	set -e # Everything must succeed
	env_setup
	while getopts c: opt; do
		case ${opt} in
			c)
				RELEASECONF="$(realpath ${OPTARG})"
				;;
			\?)
				usage
				;;
		esac
	done
	shift $(($OPTIND - 1))
	if [ ! -z "${RELEASECONF}" ]; then
		if [ -e "${RELEASECONF}" ]; then
			. ${RELEASECONF}
		else
			echo "Nonexistent configuration file: ${RELEASECONF}"
			echo "Using default build environment."
		fi
	fi
	env_check
	trap "umount ${CHROOTDIR}/dev" EXIT # Clean up devfs mount on exit
	chroot_setup
	extra_chroot_setup
	chroot_build_target
	${chroot_build_release_cmd}

	return 0
} # main()

main "${@}"
