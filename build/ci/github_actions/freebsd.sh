#!/bin/sh
set -eu

if [ $# != 1 ]
then
	echo "Usage: $0 prepare | test"
	exit 1
fi

if [ "$1" = "prepare" ]
then
	set -x -e
	env ASSUME_ALWAYS_YES=yes pkg bootstrap -f
	sed -i.bak -e 's,pkg+http://pkg.FreeBSD.org/\${ABI}/quarterly,pkg+http://pkg.FreeBSD.org/\${ABI}/latest,' /etc/pkg/FreeBSD.conf
	pkg update
	mount -u -o acls /
	mkdir /tmp_acl_nfsv4
	MD=`mdconfig -a -t swap -s 128M`
	newfs /dev/$MD
	tunefs -N enable /dev/$MD
	mount /dev/$MD /tmp_acl_nfsv4
	chmod 1777 /tmp_acl_nfsv4
	pkg install -y autoconf automake cmake gmake libiconv libtool pkgconf expat libxml2 liblz4 zstd
elif [ "$1" = "test" ]
then
	set -e
	echo "Additional NFSv4 ACL tests"
	CURDIR=`pwd`
	if [ "${BS}" = "cmake" ]
	then
		BIN_SUBDIR="bin"
	else
		BIN_SUBDIR=.
	fi
	BUILDDIR="${CURDIR}/build_ci/${BS}"
	cd "$BUILDDIR"
	TMPDIR=/tmp_acl_nfsv4 ${BIN_SUBDIR}/libarchive_test -r "${CURDIR}/libarchive/test" -v test_acl_platform_nfs4
else
	echo "Usage: $0 prepare | test"
	exit 1
fi
