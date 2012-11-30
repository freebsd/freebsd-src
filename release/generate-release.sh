#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh svn-branch[@revision] scratch-dir
#
# Environment variables (default):
#  SVNROOTBASE:	SVN base URL to FreeBSD repository (svn://svn.freebsd.org)
#  SVNROOTSRC:	URL to FreeBSD src tree (${SVNROOTBASE}/base)
#  SVNROOTDOC:	URL to FreeBSD doc tree (${SVNROOTBASE}/doc)
#  SVNROOTPORTS:URL to FreeBSD ports tree (${SVNROOTBASE}/ports)
#  BRANCHSRC:	branch name of src (svn-branch[@revision])
#  BRANCHDOC:	branch name of doc (release/9.1.0)
#  BRANCHPORTS:	branch name of ports (tags/RELEASE_9_1_0)
#  WORLD_FLAGS: optional flags to pass to buildworld (e.g. -j)
#  KERNEL_FLAGS: optional flags to pass to buildkernel (e.g. -j)
# 
#  Note: Since this requires a chroot, release cross-builds will not work!
#
# $FreeBSD$
#

usage()
{
	echo "Usage: $0 svn-branch[@revision] scratch-dir" 2>&1
	exit 1
}

if [ $# -lt 2 ]; then
	usage
fi

: ${SVNROOTBASE:=svn://svn.freebsd.org}
: ${SVNROOTSRC:=${SVNROOTBASE}/base}
: ${SVNROOTDOC:=${SVNROOTBASE}/doc}
: ${SVNROOTPORTS:=${SVNROOTBASE}/ports}
: ${SVNROOT:=${SVNROOTSRC}} # for backward compatibility
BRANCHSRC=$1
: ${BRANCHDOC:=release/9.1.0}
: ${BRANCHPORTS:=branches/RELENG_9_1_0}
: ${WORLD_FLAGS:=${MAKE_FLAGS}}
: ${KERNEL_FLAGS:=${MAKE_FLAGS}}

set -e # Everything must succeed

svn co ${SVNROOT}/${BRANCHSRC} $2/usr/src
svn co ${SVNROOTDOC}/${BRANCHDOC} $2/usr/doc
svn co ${SVNROOTPORTS}/${BRANCHPORTS} $2/usr/ports

cd $2/usr/src
make $WORLD_FLAGS buildworld
make installworld distribution DESTDIR=$2
mount -t devfs devfs $2/dev
trap "umount $2/dev" EXIT # Clean up devfs mount on exit

if [ -d $2/usr/doc ]; then 
	cp /etc/resolv.conf $2/etc/resolv.conf

	# Install docproj to build release documentation
	chroot $2 make -C /usr/ports/textproc/docproj install \
		BATCH=yes WITHOUT_X11=yes JADETEX=no WITHOUT_PYTHON=yes
fi

chroot $2 make -C /usr/src $WORLD_FLAGS buildworld
chroot $2 make -C /usr/src $KERNEL_FLAGS buildkernel
chroot $2 make -C /usr/src/release release
chroot $2 make -C /usr/src/release install DESTDIR=/R
