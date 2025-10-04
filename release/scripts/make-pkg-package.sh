#!/bin/sh

# Simulate the build environment.  Note that we need to unset some variables
# which are set in the src tree since they have different (unwanted) effects
# in the ports tree.
SRC_PKG_VERSION=${PKG_VERSION}
PKG_ABI=$(${PKG_CMD} -o ABI_FILE=${WSTAGEDIR}/bin/sh config ABI)
unset PKG_VERSION
unset MAKEFLAGS
unset PKGBASE
# Ports interprets CROSS_TOOLCHAIN differently from src, and having this set
# breaks the package-pkg build.  For now, forcibly unset this and hope ports
# can find a working compiler.
if [ -n "$CROSS_TOOLCHAIN" ]; then
	printf >&2 '%s: WARNING: CROSS_TOOLCHAIN will be ignored for the pkg build.\n' "$0"
	unset CROSS_TOOLCHAIN
fi
export WRKDIRPREFIX=/tmp/ports.${TARGET}
export DISTDIR=/tmp/distfiles
export WRKDIR=$(make -C ${PORTSDIR}/ports-mgmt/pkg I_DONT_CARE_IF_MY_BUILDS_TARGET_THE_WRONG_RELEASE=YES -V WRKDIR)

make -C ${PORTSDIR}/ports-mgmt/pkg TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH} \
	CONFIGURE_ARGS="--host=$(uname -m)-portbld-freebsd${REVISION} --prefix=${LOCALBASE}" \
	I_DONT_CARE_IF_MY_BUILDS_TARGET_THE_WRONG_RELEASE=YES \
	BATCH=YES stage create-manifest

${PKG_CMD} -o ABI=${PKG_ABI} \
	create -v -m ${WRKDIR}/.metadir.pkg/ \
	-r ${WRKDIR}/stage \
	-p ${WRKDIR}/.PLIST.mktmp \
	-o ${REPODIR}/${PKG_ABI}/${SRC_PKG_VERSION}
