#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh [-r revision] svn-branch scratch-dir
#
# Environment variables:
#  CVSUP_HOST: Host of a cvsup server to obtain the ports and documentation
#   trees. This or CVSROOT must be set to include ports and documentation.
#  CVSROOT:    CVS root to obtain the ports and documentation trees. This or
#   CVSUP_HOST must be set to include ports and documentation.
#  CVS_TAG:    CVS tag for ports and documentation (HEAD by default)
#  SVNROOT:    SVN URL to FreeBSD source repository (by default, 
#   svn://svn.freebsd.org/base)
#  MAKE_FLAGS: optional flags to pass to make (e.g. -j)
#  RELSTRING:  optional base name for media images (e.g. FreeBSD-9.0-RC2-amd64)
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $FreeBSD$
#

usage()
{
	echo "Usage: $0 [-r revision] [-d docrevision] [-p portsrevision] svn-branch scratch-dir"
	exit 1
}

REVISION=
DOCREVISION=
PORTSREVISION=
while getopts d:r:p: opt; do
	case $opt in
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

if [ $# -lt 2 ]; then
	usage
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
make $MAKE_FLAGS buildworld
make installworld distribution DESTDIR=$2
mount -t devfs devfs $2/dev
trap "umount $2/dev" EXIT # Clean up devfs mount on exit

if [ -d $2/usr/doc ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Install docproj to build release documentation
	chroot $2 /bin/sh -c '(export ASSUME_ALWAYS_YES=1 && /usr/sbin/pkg install -y docproj) || (cd /usr/ports/textproc/docproj && make install clean BATCH=yes WITHOUT_X11=yes JADETEX=no WITHOUT_PYTHON=yes)'
fi

chroot $2 make -C /usr/src $MAKE_FLAGS buildworld buildkernel
chroot $2 make -C /usr/src/release release
chroot $2 make -C /usr/src/release install DESTDIR=/R

: ${RELSTRING=`chroot $2 uname -s`-`chroot $2 uname -r`-`chroot $2 uname -p`}

cd $2/R
for i in release.iso bootonly.iso memstick; do
	mv $i $RELSTRING-$i
done
sha256 $RELSTRING-* > CHECKSUM.SHA256
md5 $RELSTRING-* > CHECKSUM.MD5

