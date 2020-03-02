#!/bin/sh
UNAME=`uname`
if [ "$1" = "prepare" ]
then
	if [ "${UNAME}" = "FreeBSD" ]
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
		pkg install -y autoconf automake cmake libiconv libtool pkgconf expat libxml2 liblz4 zstd
	elif [ "${UNAME}" = "Darwin" ]
	then
		set -x -e
		brew update > /dev/null
		for pkg in autoconf automake libtool pkg-config cmake xz lz4 zstd
		do
			brew list $pkg > /dev/null && brew upgrade $pkg || brew install $pkg
		done
	elif [ "${UNAME}" = "Linux" ]
	then
		if [ -f "/etc/debian_version" ]
		then
			apt-get -y update
			apt-get -y install build-essential locales automake libtool bison sharutils pkgconf libacl1-dev libbz2-dev zlib1g-dev liblzma-dev liblz4-dev libzstd-dev libssl-dev lrzip cmake
		elif [ -f "/etc/fedora-release" ]
		then
			dnf -y install make cmake gcc gcc-c++ kernel-devel automake libtool bison sharutils pkgconf libacl-devel librichacl-devel bzip2-devel zlib-devel xz-devel lz4-devel libzstd-devel openssl-devel
		fi
	fi
elif [ "$1" = "test" ]
then
	if [ "${UNAME}" = "FreeBSD" ]
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
	fi
else
	echo "Usage $0 prepare | test_nfsv4_acls"
	exit 1
fi
