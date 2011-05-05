#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh svn-branch scratch-dir
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
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $FreeBSD$
#

mkdir -p $2/usr/src
set -e # Everything must succeed

svn co ${SVNROOT:-svn://svn.freebsd.org/base}/$1 $2/usr/src
if [ ! -z $CVSUP_HOST ]; then
	cat > $2/docports-supfile << EOF
	*default host=$CVSUP_HOST
	*default base=/var/db
	*default prefix=/usr
	*default release=cvs tag=${CVS_TAG:-.}
	*default delete use-rel-suffix
	*default compress
	ports-all
	doc-all
EOF
elif [ ! -z $CVSROOT ]; then
	cd $2/usr
	cvs -R ${CVSARGS} -d ${CVSROOT} co -P -r ${CVS_TAG:-HEAD} ports
	cvs -R ${CVSARGS} -d ${CVSROOT} co -P -r ${CVS_TAG:-HEAD} doc
fi

cd $2/usr/src
make $MAKE_FLAGS buildworld
make installworld distribution DESTDIR=$2
mount -t devfs devfs $2/dev
trap "umount $2/dev" EXIT # Clean up devfs mount on exit

if [ ! -z $CVSUP_HOST ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Checkout ports and doc trees
	chroot $2 /usr/bin/csup /docports-supfile
fi

if [ -d $2/usr/doc ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Build ports to build the docs, then build the docs
	chroot $2 /bin/sh -c 'pkg_add -r docproj || (cd /usr/ports/textproc/docproj && make install clean BATCH=yes WITHOUT_X11=yes JADETEX=no WITHOUT_PYTHON=yes)'
	chroot $2 make -C /usr/doc $MAKE_FLAGS 'FORMATS=html html-split txt' URLS_ABSOLUTE=YES
fi

chroot $2 make -C /usr/src $MAKE_FLAGS buildworld buildkernel
chroot $2 make -C /usr/src/release obj
chroot $2 make -C /usr/src/release release
chroot $2 make -C /usr/src/release install DESTDIR=/R

