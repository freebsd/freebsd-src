#!/bin/sh
#
# $FreeBSD$
#

set -e

usage() {
	echo "$(basename ${0}) /path/to/pkg-stage.conf revision"
	exit 1
}

if [ ! -e "${1}" ]; then
	echo "Configuration file not specified."
	echo
	usage
fi

if [ "$#" -lt 2 ]; then
	usage
fi

# Source config file for this architecture.
REVISION="${2}"
. "${1}" || exit 1

if [ ! -x /usr/local/sbin/pkg ]; then
	/usr/bin/make -C /usr/ports/ports-mgmt/pkg install clean
fi

/bin/mkdir -p ${PKG_CACHEDIR}

${PKGCMD} update -f
${PKGCMD} fetch -d ${DVD_PACKAGES}

${PKGCMD} repo ${PKG_CACHEDIR}

# Always exit '0', even if pkg(8) complains about conflicts.
exit 0
