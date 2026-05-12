#!/bin/sh
#
# Install specified libcrypto.
#  -a : install version for ABI compatibility test.
#  -n : dry run, don't actually build and install.
#
# Usage: $0 [-a] [-n] openssl-$branch/tag destdir [config options]

set -e

bincompat_test=""
dryrun=""
while [ "$1" = "-a" ] || [ "$1" = "-n" ]; do
	if [ "$1" = "-a" ]; then
		abi_compat_test=y
	elif [ "$1" = "-n" ]; then
		dryrun="echo dryrun:"
	fi
	shift
done

ver="$1"
destdir="$2"
opts="$3"

if [ -z "${ver}" ] || [ -z "${destdir}" ]; then
	echo tag/branch and destdir required
	exit 1
fi

set -x

if [ ! -d ${HOME}/openssl ]; then
	cd ${HOME}
	git clone https://github.com/openssl/openssl.git
	cd ${HOME}/openssl
	git fetch --all
fi
cd ${HOME}/openssl

if [ "${abi_compat_test}" = "y" ]; then
	echo selecting ABI test release/branch for ${ver}
	case "${ver}" in
	openssl-3.6)
		ver=openssl-3.0.0
		echo "selecting older release ${ver}"
		;;
	openssl-3.[012345])
		major=$(echo ${ver} | cut -f1 -d.)
		minor=$(echo ${ver} | cut -f2 -d.)
		ver="${major}.$((${minor} + 1))"
		echo selecting next release branch ${ver}
		;;
	openssl-3.*.*)
		major=$(echo ${ver} | cut -f1 -d.)
		minor=$(echo ${ver} | cut -f2 -d.)
		patch=$(echo ${ver} | cut -f3 -d.)
		ver="${major}.${minor}.$((${patch} + 1))"
		echo checking for release tag ${ver}
		if git tag | grep -q "^${ver}\$"; then
			echo selected next patch release ${ver}
		else
			ver="${major}.${minor}"
			echo not found, selecting release branch ${ver}
		fi
		;;
	esac
fi

git checkout ${ver}
make clean >/dev/null 2>&1 || true
${dryrun} ./config no-threads shared ${opts} --prefix=${destdir} \
    -Wl,-rpath,${destdir}/lib64
${dryrun} make -j4
${dryrun} sudo make install_sw
