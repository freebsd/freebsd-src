#!/bin/sh
#-
# Copyright (c) 2013 Glen Barber
# Copyright (c) 2011 Nathan Whitehorn
# All rights reserved.
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
# $FreeBSD$
#

PATH="/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin"
export PATH

# The directory within which the release will be built.
CHROOTDIR="/scratch"

# The default svn checkout server, and svn branches for src/, doc/,
# and ports/.
SVNROOT="svn://svn.freebsd.org"
SRCBRANCH="base/head"
DOCBRANCH="doc/head"
PORTBRANCH="ports/head"

# Sometimes one needs to checkout src with --force svn option.
# If custom kernel configs copied to src tree before checkout, e.g.
SRC_FORCE_CHECKOUT=

# The default src/, doc/, and ports/ revisions.
SRCREVISION="-rHEAD"
DOCREVISION="-rHEAD"
PORTREVISION="-rHEAD"

# The default make.conf and src.conf to use.  Set to /dev/null
# by default to avoid polluting the chroot(8) environment with
# non-default settings.
MAKE_CONF="/dev/null"
SRC_CONF="/dev/null"

# The number of make(1) jobs, defaults to the number of CPUs available for
# buildworld, and half of number of CPUs available for buildkernel.
WORLD_FLAGS="-j$(sysctl -n hw.ncpu)"
KERNEL_FLAGS="-j$(expr $(sysctl -n hw.ncpu) / 2)"
MAKE_FLAGS="-s"

# The name of the kernel to build, defaults to GENERIC.
KERNEL="GENERIC"

# The TARGET and TARGET_ARCH to build, defaults to the running system.
TARGET="$(uname -p)"
TARGET_ARCH="${TARGET}"

# Set to non-empty value to disable checkout of doc/ and/or ports/.  Disabling
# ports/ checkout also forces NODOC to be set.
NODOC=
NOPORTS=
MAKE_FLAGS="${MAKE_FLAGS}"

get_rev_branch () {
	# Set up the OSVERSION, BRANCH, and REVISION based on the src/ tree
	# checked out.
	OSVERSION=$(grep '#define __FreeBSD_version' ${CHROOTDIR}/usr/src/sys/sys/param.h | awk '{print $3}')
	BRANCH=$(grep '^BRANCH=' ${CHROOTDIR}/usr/src/sys/conf/newvers.sh \
		| awk -F\= '{print $2}' | sed -e 's,",,g')
	REVISION=$(grep '^REVISION=' ${CHROOTDIR}/usr/src/sys/conf/newvers.sh \
		| awk -F\= '{print $2}' | sed -e 's,",,g')
	OSRELEASE="${REVISION}-${BRANCH}"
}

usage() {
	echo "Usage: $0 [-c release.conf]"
	exit 1
}

while getopts c: opt; do
	case ${opt} in
	c)
		RELEASECONF="${OPTARG}"
		if [ ! -e "${RELEASECONF}" ]; then
			echo "ERROR: Configuration file ${RELEASECONF} does not exist."
			exit 1
		fi
		# Source the specified configuration file for overrides
		. ${RELEASECONF}
		;;
	\?)
		usage
		;;
	esac
done
shift $(($OPTIND - 1))

# If PORTS is set and NODOC is unset, force NODOC=yes because the ports tree
# is required to build the documentation set.
if [ "x${NOPORTS}" != "x" ] && [ "x${NODOC}" = "x" ]; then
	echo "*** NOTICE: Setting NODOC=1 since ports tree is required"
	echo "            and NOPORTS is set."
	NODOC=yes
fi

# If NOPORTS and/or NODOC are unset, they must not pass to make as variables.
# The release makefile verifies definedness of NOPORTS/NODOC variables
# instead of their values.
DOCPORTS=
if [ "x${NOPORTS}" != "x" ]; then
 DOCPORTS="NOPORTS=yes "
fi
if [ "x${NODOC}" != "x" ]; then
 DOCPORTS="${DOCPORTS}NODOC=yes"
fi

# The aggregated build-time flags based upon variables defined within
# this file, unless overridden by release.conf.  In most cases, these
# will not need to be changed.
CONF_FILES="__MAKE_CONF=${MAKE_CONF} SRCCONF=${SRC_CONF}"
ARCH_FLAGS="TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH}"
CHROOT_WMAKEFLAGS="${MAKE_FLAGS} ${WORLD_FLAGS} ${CONF_FILES}"
CHROOT_IMAKEFLAGS="${CONF_FILES}"
CHROOT_DMAKEFLAGS="${CONF_FILES}"
RELEASE_WMAKEFLAGS="${MAKE_FLAGS} ${WORLD_FLAGS} ${ARCH_FLAGS} ${CONF_FILES}"
RELEASE_KMAKEFLAGS="${MAKE_FLAGS} ${KERNEL_FLAGS} KERNCONF=\"${KERNEL}\" ${ARCH_FLAGS} ${CONF_FILES}"
RELEASE_RMAKEFLAGS="${ARCH_FLAGS} KERNCONF=\"${KERNEL}\" ${CONF_FILES} \
	${DOCPORTS}"

# Force src checkout if configured
FORCE_SRC_KEY=
if [ "x${SRC_FORCE_CHECKOUT}" != "x" ]; then
 FORCE_SRC_KEY="--force"
fi

if [ ! ${CHROOTDIR} ]; then
	echo "Please set CHROOTDIR."
	exit 1
fi

if [ $(id -u) -ne 0 ]; then
	echo "Needs to be run as root."
	exit 1
fi

set -e # Everything must succeed

mkdir -p ${CHROOTDIR}/usr

svn co ${FORCE_SRC_KEY} ${SVNROOT}/${SRCBRANCH} ${CHROOTDIR}/usr/src $SRCREVISION
if [ "x${NODOC}" = "x" ]; then
	svn co ${SVNROOT}/${DOCBRANCH} ${CHROOTDIR}/usr/doc $DOCREVISION
fi
if [ "x${NOPORTS}" = "x" ]; then
	svn co ${SVNROOT}/${PORTBRANCH} ${CHROOTDIR}/usr/ports $PORTREVISION
fi

get_rev_branch

cd ${CHROOTDIR}/usr/src
make ${CHROOT_WMAKEFLAGS} buildworld
make ${CHROOT_IMAKEFLAGS} installworld DESTDIR=${CHROOTDIR}
make ${CHROOT_DMAKEFLAGS} distribution DESTDIR=${CHROOTDIR}
mount -t devfs devfs ${CHROOTDIR}/dev
trap "umount ${CHROOTDIR}/dev" EXIT # Clean up devfs mount on exit

build_doc_ports() {
	## Trick the ports 'run-autotools-fixup' target to do the right thing.
	_OSVERSION=$(sysctl -n kern.osreldate)
	if [ -d ${CHROOTDIR}/usr/doc ] && [ "x${NODOC}" = "x" ]; then
		PBUILD_FLAGS="OSVERSION=${_OSVERSION} WITHOUT_JADETEX=yes WITHOUT_X11=yes BATCH=yes"
		chroot ${CHROOTDIR} make -C /usr/ports/textproc/docproj \
			${PBUILD_FLAGS} install clean distclean
	fi
}

# If MAKE_CONF and/or SRC_CONF are set and not character devices (/dev/null),
# copy them to the chroot.
if [ -e ${MAKE_CONF} ] && [ ! -c ${MAKE_CONF} ]; then
	mkdir -p ${CHROOTDIR}/$(dirname ${MAKE_CONF})
	cp ${MAKE_CONF} ${CHROOTDIR}/${MAKE_CONF}
fi
if [ -e ${SRC_CONF} ] && [ ! -c ${SRC_CONF} ]; then
	mkdir -p ${CHROOTDIR}/$(dirname ${SRC_CONF})
	cp ${SRC_CONF} ${CHROOTDIR}/${SRC_CONF}
fi

if [ -d ${CHROOTDIR}/usr/ports ]; then
	cp /etc/resolv.conf ${CHROOTDIR}/etc/resolv.conf
	build_doc_ports ${CHROOTDIR}
fi

if [ "x${RELSTRING}" = "x" ]; then
	RELSTRING="$(chroot ${CHROOTDIR} uname -s)-${OSRELEASE}-${TARGET_ARCH}"
fi

eval chroot ${CHROOTDIR} make -C /usr/src ${RELEASE_WMAKEFLAGS} buildworld
eval chroot ${CHROOTDIR} make -C /usr/src ${RELEASE_KMAKEFLAGS} buildkernel
eval chroot ${CHROOTDIR} make -C /usr/src/release ${RELEASE_RMAKEFLAGS} \
	release RELSTRING=${RELSTRING}
eval chroot ${CHROOTDIR} make -C /usr/src/release ${RELEASE_RMAKEFLAGS} \
	install DESTDIR=/R RELSTRING=${RELSTRING}

cd ${CHROOTDIR}/R

sha256 FreeBSD-* > CHECKSUM.SHA256
md5 FreeBSD-* > CHECKSUM.MD5
