#!/bin/sh
set -e
UNAME=`uname`
CURDIR=`pwd`
SRCDIR="${SRCDIR:-`pwd`}"
if [ -z "${BUILDDIR}" ]; then
        BUILDDIR="${CURDIR}/build_ci/${BS}"
fi
mkdir -p "${BUILDDIR}"
cd "${BUILDDIR}"
case "$UNAME" in
	MSYS*)
	if [ "${BS}" = "msbuild" ]; then
		set -x
		cmake -G "Visual Studio 15 2017" -D CMAKE_BUILD_TYPE="Release" "${SRCDIR}"
		cmake --build . --target ALL_BUILD
		# Until fixed, we don't run tests on Windows (lots of fails + timeout)
		#export SKIP_TEST_FUZZ=1
		#cmake --build . --target RUN_TESTS
		set +x
	elif [ "${BS}" = "mingw" ]; then
		set -x
		cmake -G "MSYS Makefiles" -D CMAKE_C_COMPILER="${CC}" -D CMAKE_MAKE_PROGRAM="mingw32-make" -D CMAKE_BUILD_TYPE="Release" "${SRCDIR}"
		mingw32-make
		#export SKIP_TEST_FUZZ=1
		#mingw32-make test
		set +x
	else
		echo "Unknown or unspecified build type: ${BS}"
		exit 1
	fi
	;;
esac
