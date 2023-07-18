#!/bin/sh
#
# Automated build and test of libarchive on CI systems
#
# Variables that can be passed via environment:
# BS=			# build system (autotools or cmake)
# CRYPTO=		# cryptography provider (openssl, nettle or mbedtls)
# BUILDDIR=		# build directory
# SRCDIR=		# source directory
# CONFIGURE_ARGS=	# configure arguments
# CMAKE_ARGS=		# cmake arguments
# MAKE_ARGS=		# make arguments
# DEBUG=		# set -g -fsanitize=address flags

ACTIONS=
if [ -n "${BUILD_SYSTEM}" ]; then
	BS="${BUILD_SYSTEM}"
fi

BS="${BS:-autotools}"
MAKE="${MAKE:-make}"
CMAKE="${CMAKE:-cmake}"
CURDIR=`pwd`
SRCDIR="${SRCDIR:-`pwd`}"
RET=0

usage () {
	echo "Usage: $0 [-b autotools|cmake] [-a autogen|configure|build|test|install|distcheck ] [ -a ... ] [ -d builddir ] [-c openssl|nettle|mbedtls] [-s srcdir ]"
}
inputerror () {
	echo $1
	usage
	exit 1
}
while getopts a:b:c:d:s: opt; do
	case ${opt} in
		a)
			case "${OPTARG}" in
				autogen) ;;
				configure) ;;
				build) ;;
				test) ;;
				install) ;;
				distcheck) ;;
				artifact) ;;
				dist-artifact) ;;
				*) inputerror "Invalid action (-a)" ;;
			esac
			ACTIONS="${ACTIONS} ${OPTARG}"
		;;
		b) BS="${OPTARG}"
			case "${BS}" in
				autotools) ;;
				cmake) ;;
				*) inputerror "Invalid build system (-b)" ;;
			esac
		;;
		c) CRYPTO="${OPTARG}"
			case "${CRYPTO}" in
				mbedtls) ;;
				openssl) ;;
				nettle) ;;
				*) inputerror "Invalid crypto provider (-c)" ;;
			esac
		;;
		d)
			BUILDDIR="${OPTARG}"
		;;
		s)
			SRCDIR="${OPTARG}"
			if [ ! -f "${SRCDIR}/build/version" ]; then
				inputerror "Missing file: ${SRCDIR}/build/version"
			fi
		;;
	esac
done
case "${CRYPTO}" in
	mbedtls)
		CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_OPENSSL=OFF -DENABLE_MBEDTLS=ON"
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --without-openssl --with-mbedtls"
	;;
	nettle)
		CMAKE_ARGS="${CMAKE_ARGS} -DENABLE_OPENSSL=OFF -DENABLE_NETTLE=ON"
		CONFIGURE_ARGS="${CONFIGURE_ARGS} --without-openssl --with-nettle"
	;;
esac
if [ -z "${MAKE_ARGS}" ]; then
	if [ "${BS}" = "autotools" ]; then
		MAKE_ARGS="V=1"
	elif [ "${BS}" = "cmake" ]; then
		MAKE_ARGS="VERBOSE=1"
	fi
fi
if [ -n "${DEBUG}" ]; then
	if [ -n "${CFLAGS}" ]; then
		export CFLAGS="${CFLAGS} -g -fsanitize=address"
	else
		export CFLAGS="-g -fsanitize=address"
	fi
fi
if [ -z "${ACTIONS}" ]; then
	ACTIONS="autogen configure build test install"
fi
if [ -z "${BS}" ]; then
	inputerror "Missing build system (-b) parameter"
fi
if [ -z "${BUILDDIR}" ]; then
	BUILDDIR="${CURDIR}/build_ci/${BS}"
fi
mkdir -p "${BUILDDIR}"
for action in ${ACTIONS}; do
	cd "${BUILDDIR}"
	case "${action}" in
		autogen)
			case "${BS}" in
				autotools)
					cd "${SRCDIR}"
					sh build/autogen.sh
					RET="$?"
				;;
			esac
		;;
		configure)
			case "${BS}" in
				autotools) "${SRCDIR}/configure" ${CONFIGURE_ARGS} ;;
				cmake) ${CMAKE} ${CMAKE_ARGS} "${SRCDIR}" ;;
			esac
			RET="$?"
		;;
		build)
			${MAKE} ${MAKE_ARGS}
			RET="$?"
		;;
		test)
			case "${BS}" in
				autotools)
					${MAKE} ${MAKE_ARGS} check LOG_DRIVER="${SRCDIR}/build/ci/test_driver"
					;;
				cmake)
					${MAKE} ${MAKE_ARGS} test
					;;
			esac
			RET="$?"
			find ${TMPDIR:-/tmp} -path '*_test.*' -name '*.log' -print -exec cat {} \;
		;;
		install)
			${MAKE} ${MAKE_ARGS} install DESTDIR="${BUILDDIR}/destdir"
			RET="$?"
			cd "${BUILDDIR}/destdir" && ls -lR .
		;;
		distcheck)
			${MAKE} ${MAKE_ARGS} distcheck || (
				RET="$?"
				find . -name 'test-suite.log' -print -exec cat {} \;
				find ${TMPDIR:-/tmp} -path '*_test.*' -name '*.log' -print -exec cat {} \;
				exit "${RET}"
			)
			RET="$?"
		;;
		artifact)
			tar -c -J -C "${BUILDDIR}/destdir" -f "${CURDIR}/libarchive.tar.xz" usr
			ls -l "${CURDIR}/libarchive.tar.xz"
		;;
		dist-artifact)
			tar -c -C "${BUILDDIR}" -f "${CURDIR}/libarchive-dist.tar" \
				libarchive-*.tar.gz libarchive-*.tar.xz libarchive-*.zip
			ls -l "${CURDIR}/libarchive-dist.tar"
		;;
	esac
	if [ "${RET}" != "0" ]; then
		exit "${RET}"
	fi
	cd "${CURDIR}"
done
exit "${RET}"
