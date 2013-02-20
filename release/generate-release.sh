#!/bin/sh

# generate-release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
#
#  Usage: generate-release.sh svn-branch[@revision] scratch-dir
#
# Environment variables:
#  SVNROOTBASE: SVN base URL to FreeBSD repository (svn://svn.freebsd.org)
#  SVNROOTSRC:  URL to FreeBSD src tree (${SVNROOTBASE}/base)
#  SVNROOTDOC:  URL to FreeBSD doc tree (${SVNROOTBASE}/doc)
#  SVNROOTPORTS:URL to FreeBSD ports tree (${SVNROOTBASE}/ports)
#  BRANCHSRC:   branch name of src (svn-branch[@revision])
#  BRANCHDOC:   branch name of doc (head)
#  BRANCHPORTS: branch name of ports (head)
#  WORLD_FLAGS: optional flags to pass to buildworld (e.g. -j)
#  KERNEL_FLAGS: optional flags to pass to buildkernel (e.g. -j)
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
: ${SVN_CMD:=/usr/local/bin/svn}
BRANCHSRC=$1
: ${BRANCHDOC:=head}
: ${BRANCHPORTS:=head}
: ${WORLD_FLAGS:=${MAKE_FLAGS}}
: ${KERNEL_FLAGS:=${MAKE_FLAGS}}
: ${CHROOTDIR:=$2}
 
if [ ! -r "${CHROOTDIR}" ]; then
	echo "${CHROOTDIR}: scratch dir not found."
	exit 1
fi

CHROOT_CMD="/usr/sbin/chroot ${CHROOTDIR}"
case ${TARGET} in
"")	;;
*)	SETENV_TARGET="TARGET=$TARGET" ;;
esac
case ${TARGET_ARCH} in
"")	;;
*)	SETENV_TARGET_ARCH="TARGET_ARCH=$TARGET_ARCH" ;;
esac
SETENV="env -i PATH=/bin:/usr/bin:/sbin:/usr/sbin"
CROSSENV="${SETENV_TARGET} ${SETENV_TARGET_ARCH}"
WMAKE="make -C /usr/src ${WORLD_FLAGS}"
NWMAKE="${WMAKE} __MAKE_CONF=/dev/null SRCCONF=/dev/null"
KMAKE="make -C /usr/src ${KERNEL_FLAGS}"
RMAKE="make -C /usr/src/release"

if [ $(id -u) -ne 0 ]; then
	echo "Needs to be run as root."
	exit 1
fi

set -e # Everything must succeed

mkdir -p ${CHROOTDIR}/usr/src
${SVN_CMD} co ${SVNROOT}/${BRANCHSRC} ${CHROOTDIR}/usr/src
${SVN_CMD} co ${SVNROOTDOC}/${BRANCHDOC} ${CHROOTDIR}/usr/doc
${SVN_CMD} co ${SVNROOTPORTS}/${BRANCHPORTS} ${CHROOTDIR}/usr/ports

${SETENV} ${NWMAKE} -C ${CHROOTDIR}/usr/src ${WORLD_FLAGS} buildworld
${SETENV} ${NWMAKE} -C ${CHROOTDIR}/usr/src installworld distribution DESTDIR=${CHROOTDIR}
mount -t devfs devfs ${CHROOTDIR}/dev
trap "umount ${CHROOTDIR}/dev" EXIT # Clean up devfs mount on exit

if [ -d ${CHROOTDIR}/usr/doc ]; then 
	cp /etc/resolv.conf ${CHROOTDIR}/etc/resolv.conf

	# Install docproj to build release documentation
	${CHROOT_CMD} /bin/sh -c \
		'make -C /usr/ports/textproc/docproj \
			BATCH=yes \
			WITHOUT_SVN=yes \
			WITHOUT_JADETEX=yes \
			WITHOUT_X11=yes \
			WITHOUT_PYTHON=yes \
			install'
fi

${CHROOT_CMD} ${SETENV} ${CROSSENV} ${WMAKE} buildworld
${CHROOT_CMD} ${SETENV} ${CROSSENV} ${KMAKE} buildkernel
${CHROOT_CMD} ${SETENV} ${CROSSENV} ${RMAKE} release
${CHROOT_CMD} ${SETENV} ${CROSSENV} ${RMAKE} install DESTDIR=/R
