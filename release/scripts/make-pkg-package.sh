#!/bin/sh

# Simulate the build environment.  Note that we need to unset some variables
# which are set in the src tree since they have different (unwanted) effects
# in the ports tree.
SRC_PKG_VERSION=${PKG_VERSION}
PKG_ABI=$(${PKG_CMD} -o ABI_FILE=${WSTAGEDIR}/bin/sh config ABI)
unset PKG_VERSION
unset MAKEFLAGS
unset PKGBASE
export WRKDIRPREFIX=/tmp/ports.${TARGET}
export WRKDIR=$(make -C ${PORTSDIR}/ports-mgmt/pkg -V WRKDIR)

make -C ${PORTSDIR}/ports-mgmt/pkg TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH} \
	CONFIGURE_ARGS="--host=$(uname -m)-portbld-freebsd${REVISION}" \
	I_DONT_CARE_IF_MY_BUILDS_TARGET_THE_WRONG_RELEASE=YES \
	BATCH=YES stage create-manifest

${PKG_CMD} -o ABI=${PKG_ABI} \
	create -v -m ${WRKDIR}/.metadir.pkg/ \
	-r ${WRKDIR}/stage \
	-p ${WRKDIR}/.PLIST.mktmp \
	-o ${REPODIR}/${PKG_ABI}/${SRC_PKG_VERSION}
