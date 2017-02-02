#!/bin/sh
#
# Automated build and test of libarchive on CI systems
#
# Variables that can be passed via environment:
# BUILD_SYSTEM=
# BUILDDIR=
# SRCDIR=
# CONFIGURE_ARGS=
# MAKE_ARGS=
#

ACTIONS=
BUILD_SYSTEM="${BUILD_SYSTEM:-autotools}"
MAKE="${MAKE:-make}"
CMAKE="${CMAKE:-cmake}"
CURDIR=`pwd`
SRCDIR="${SRCDIR:-`pwd`}"
RET=0

usage () {
	echo "Usage: $0 [-b autotools|cmake] [-a autogen|configure|build|test ] [ -a ... ] [ -d builddir ] [-s srcdir ]"
}
inputerror () {
	echo $1
	usage
	exit 1
}
while getopts a:b:d:s: opt; do
	case ${opt} in
		a)
			case "${OPTARG}" in
				autogen) ;;
				configure) ;;
				build) ;;
				test) ;;
				*) inputerror "Invalid action (-a)" ;;
			esac
			ACTIONS="${ACTIONS} ${OPTARG}"
		;;
		b) BUILD_SYSTEM="${OPTARG}"
			case "${BUILD_SYSTEM}" in
				autotools) ;;
				cmake) ;;
				*) inputerror "Invalid build system (-b)" ;;
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
if [ -z "${ACTIONS}" ]; then
	ACTIONS="autogen configure build test"
fi
if [ -z "${BUILD_SYSTEM}" ]; then
	inputerror "Missing type (-t) parameter"
fi
if [ -z "${BUILDDIR}" ]; then
	BUILDDIR="${CURDIR}/build_ci/${BUILD_SYSTEM}"
fi
mkdir -p "${BUILDDIR}"
for action in ${ACTIONS}; do
	cd "${BUILDDIR}"
	case "${action}" in
		autogen)
			case "${BUILD_SYSTEM}" in
				autotools)
					cd "${SRCDIR}"
					sh build/autogen.sh
					RET="$?"
				;;
			esac
		;;
		configure)
			case "${BUILD_SYSTEM}" in
				autotools) "${SRCDIR}/configure" ${CONFIGURE_ARGS} ;;
				cmake) ${CMAKE} ${CONFIGURE_ARGS} "${SRCDIR}" ;;
			esac
			RET="$?"
		;;
		build)
			${MAKE} ${MAKE_ARGS}
			RET="$?"
		;;
		test)
			case "${BUILD_SYSTEM}" in
				autotools)
					${MAKE} ${MAKE_ARGS} check LOG_DRIVER="${SRCDIR}/build/ci_test_driver"
					;;
				cmake)
					${MAKE} ${MAKE_ARGS} test
					;;
			esac
			RET="$?"
		;;
	esac
	if [ "${RET}" != "0" ]; then
		exit "${RET}"
	fi
	cd "${CURDIR}"
done
exit "${RET}"
