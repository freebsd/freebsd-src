#!/bin/sh
#
# $FreeBSD$
#

# Simulate the build environment.
eval $(make -C ${SRCDIR} TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH} buildenvvars)
export WRKDIRPREFIX=/tmp/ports.${TARGET}
export WSTAGEDIR=${WSTAGEDIR}
export REPODIR=${REPODIR}
export PKG_VERSION=${PKG_VERSION}
export WRKDIR=$(make -C ${PORTSDIR}/ports-mgmt/pkg -V WRKDIR)

make -C ${PORTSDIR}/ports-mgmt/pkg TARGET=${TARGET} TARGET_ARCH=${TARGET_ARCH} \
	CONFIGURE_ARGS="--host=$(uname -m)-portbld-freebsd${REVISION}" \
	stage create-manifest

pkg -o ABI_FILE=${WSTAGEDIR}/bin/sh \
	create -v -m ${WRKDIR}/.metadir/ \
	-r ${WRKDIR}/stage \
	-p ${WRKDIR}/.PLIST.mktmp \
	-o ${REPODIR}/$(pkg -o ABI_FILE=${WSTAGEDIR}/bin/sh config ABI)/${PKG_VERSION}
mkdir -p ${REPODIR}/$(pkg -o ABI_FILE=${WSTAGEDIR}/bin/sh config ABI)/${PKG_VERSION}/Latest/
cd ${REPODIR}/$(pkg -o ABI_FILE=${WSTAGEDIR}/bin/sh config ABI)/${PKG_VERSION}/Latest/ && \
	ln -s ../pkg-*.txz
