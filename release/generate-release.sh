#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh svn-branch scratch-dir
#
# Environment variables:
#  CVSUP_HOST: Host of a cvsup server to obtain the ports and documentation
#   trees. Must be set to include ports and documentation.
#  CVSUP_TAG:  CVS tag for ports and documentation (HEAD by default)
#  MAKE_FLAGS: optional flags to pass to make (e.g. -j)
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $FreeBSD$
#

mkdir -p $2/usr/src
svn co svn://svn.freebsd.org/base/$1 $2/usr/src || exit 1
if [ ! -z $CVSUP_HOST ]; then
	cat > $2/docports-supfile << EOF
	*default host=$CVSUP_HOST
	*default base=/var/db
	*default prefix=/usr
	*default release=cvs tag=${CVSUP_TAG:-.}
	*default delete use-rel-suffix
	*default compress
	ports-all
	doc-all
EOF
else
	RELEASE_FLAGS="-DNOPORTS -DNODOC"
fi

cd $2/usr/src
make $MAKE_FLAGS buildworld || exit 1
make installworld distribution DESTDIR=$2 || exit 1
mount -t devfs devfs $2/dev

if [ ! -z $CVSUP_HOST ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Checkout ports and doc trees
	chroot $2 /usr/bin/csup /docports-supfile || exit 1

	# Build ports to build the docs, then build the docs
	chroot $2 /bin/sh -c 'pkg_add -r docproj || (cd /usr/ports/textproc/docproj && make install clean BATCH=yes WITHOUT_X11=yes JADETEX=no WITHOUT_PYTHON=yes)' || exit 1
	chroot $2 /bin/sh -c "cd /usr/doc && make $MAKE_FLAGS 'FORMATS=html html-split txt' URLS_ABSOLUTE=YES" || exit 1
fi

chroot $2 /bin/sh -c "cd /usr/src && make $MAKE_FLAGS buildworld buildkernel" || exit 1
chroot $2 /bin/sh -c "cd /usr/src/release && make -f Makefile.bsdinstall obj release $RELEASE_FLAGS" || exit 1
mkdir $2/R
cp -pRP $2/usr/obj/usr/src/release/release.iso $2/usr/obj/usr/src/release/memstick $2/usr/obj/usr/src/release/ftp $2/R

