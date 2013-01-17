#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh [-r revision] [-d docrevision] \
#	[-p portsrevision] svn-branch scratch-dir
#
# Environment variables:
#  SVNROOT:    SVN URL to FreeBSD source repository (by default, 
#   svn://svn.freebsd.org/base)
#  MAKE_FLAGS: optional flags to pass to make (e.g. -j)
#  RELSTRING:  optional base name for media images (e.g. FreeBSD-9.0-RC2-amd64)
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $FreeBSD$
#

unset B_ARCH
unset ARCH
unset MACHINE_ARCH

HOST_ARCH=`uname -p`

usage()
{
	echo "Usage: $0 [-a arch] [-r revision] [-d docrevision] [-p portsrevision] svn-branch scratch-dir"
	exit 1
}

arch_error ()
{
	echo "Architecture ${OPTARG} cannot be built on host architecture ${HOST_ARCH}"
	exit 1
}

REVISION=
DOCREVISION=
PORTSREVISION=
while getopts a:d:r:p: opt; do
	case $opt in
	a)
		case "${OPTARG}" in
			i386|amd64)
				if [ "${HOST_ARCH}" != "amd64" ]; then
					arch_error "${OPTARG}"
				fi
				;;
			powerpc|powerpc64)
				if [ "${HOST_ARCH}" != "powerpc64" ]; then
					arch_error "${OPTARG}"
				fi
				;;
			*)
				arch_error "${OPTARG}"
				;;
		esac
		B_ARCH="$OPTARG"
		;;
	d)
		DOCREVISION="-r $OPTARG"
		;;
	r)
		REVISION="-r $OPTARG"
		;;
	p)
		PORTSREVISION="-r $OPTARG"
		;;
	\?)
		usage
		;;
	esac
done
shift $(($OPTIND - 1))

# If target architecture is not specified, use hw.machine_arch
if [ "x${B_ARCH}" == "x" ]; then
	B_ARCH="${HOST_ARCH}"
fi
ARCH_FLAGS="ARCH=${B_ARCH} TARGET_ARCH=${B_ARCH}"

if [ $# -lt 2 ]; then
	usage
fi

if [ $(id -u) -ne 0 ]; then
	echo "Needs to be run as root."
	exit 1
fi

set -e # Everything must succeed

case $MAKE_FLAGS in
	*-j*)
		;;
	*)
		MAKE_FLAGS="$MAKE_FLAGS -j "$(sysctl -n hw.ncpu)
		;;
esac

mkdir -p $2/usr/src

svn co ${SVNROOT:-svn://svn.freebsd.org/base}/$1 $2/usr/src $REVISION
svn co ${SVNROOT:-svn://svn.freebsd.org/doc}/head $2/usr/doc $DOCREVISION
svn co ${SVNROOT:-svn://svn.freebsd.org/ports}/head $2/usr/ports $PORTSREVISION

cd $2/usr/src
make $MAKE_FLAGS ${ARCH_FLAGS} buildworld
make $ARCH_FLAGS installworld distribution DESTDIR=$2
mount -t devfs devfs $2/dev
trap "umount $2/dev" EXIT # Clean up devfs mount on exit

# Most commands below are run in chroot, so fake getosreldate(3) right now
OSVERSION=$(grep '#define __FreeBSD_version' $2/usr/include/sys/param.h | awk '{print $3}')
export OSVERSION
BRANCH=$(grep '^BRANCH=' $2/usr/src/sys/conf/newvers.sh | awk -F\= '{print $2}')
BRANCH=`echo ${BRANCH} | sed -e 's,",,g'`
REVISION=$(grep '^REVISION=' $2/usr/src/sys/conf/newvers.sh | awk -F\= '{print $2}')
REVISION=`echo ${REVISION} | sed -e 's,",,g'`
OSRELEASE="${REVISION}-${BRANCH}"

pkgng_install_docports ()
{
	# Attempt to install docproj port from pkgng package.
	chroot ${CHROOTDIR} /bin/sh -c 'env ASSUME_ALWAYS_YES=1 /usr/sbin/pkg install -y docproj-nojadetex'
	# Check if docproj was installed, since pkg(8) returns '0' if unable
	# to install a package from the repository.  If it is not installed,
	# fallback to installing using pkg_add(1).
	chroot ${CHROOTDIR} /bin/sh -c '/usr/sbin/pkg info -q docproj-nojadetex' || \
		pkgadd_install_docports
}

build_compat9_port ()
{
	chroot ${CHROOTDIR} /bin/sh -c 'make -C /usr/ports/misc/compat9x BATCH=yes install clean'
}

pkgadd_install_docports ()
{
	# Attempt to install docproj package with pkg_add(1).
	# If not successful, build the docproj port.
	if [ "${REVISION}" == "10.0" ]; then
		# Packages for 10-CURRENT are still located in the 9-CURRENT
		# directory.  Override environment to use correct package
		# location if building for 10-CURRENT.
		PACKAGESITE="ftp://ftp.freebsd.org/pub/FreeBSD/ports/${B_ARCH}/packages-9-current/Latest/"
		export PACKAGESITE
		PACKAGEROOT="ftp://ftp.freebsd.org/pub/FreeBSD/ports/${B_ARCH}/packages-9-current/"
		export PACKAGEROOT
		PKG_PATH="ftp://ftp.freebsd.org/pub/FreeBSD/ports/${B_ARCH}/packages-9-current/All/"
		export PKG_PATH
		build_compat9_port
	fi
	chroot ${CHROOTDIR} /bin/sh -c '/usr/sbin/pkg_add -r docproj-nojadetex' || \
		build_docports
}

build_docports() 
{
	# Could not install textproc/docproj from pkg(8) or pkg_add(1).  Build
	# the port as final fallback.
	chroot ${CHROOTDIR} /bin/sh -c 'make -C /usr/ports/textproc/docproj BATCH=yes WITHOUT_SVN=yes WITH_JADETEX=no WITHOUT_X11=yes WITHOUT_PYTHON=yes install clean' || \
		{ echo "*** Could not build the textproj/docproj port.  Exiting."; exit 2; }
}

if [ -d $2/usr/doc ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Install docproj to build release documentation
	CHROOTDIR="$2"
	set +e
	pkgng_install_docports "${CHROOTDIR}"
	set -e
fi

chroot $2 make -C /usr/src $MAKE_FLAGS ${ARCH_FLAGS} buildworld buildkernel
chroot $2 make -C /usr/src/release ${ARCH_FLAGS} release
chroot $2 make -C /usr/src/release install DESTDIR=/R

if [ "x${OSVERSION}" == "x" ]; then
	OSRELEASE=`chroot $2 uname -r`
fi

: ${RELSTRING=`chroot $2 uname -s`-${OSRELEASE}-${B_ARCH}}

cd $2/R
for i in release.iso bootonly.iso memstick; do
	mv $i $RELSTRING-$i
done
sha256 $RELSTRING-* > CHECKSUM.SHA256
md5 $RELSTRING-* > CHECKSUM.MD5

