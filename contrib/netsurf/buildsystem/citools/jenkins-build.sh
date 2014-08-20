#!/bin/bash
#
# Copyright © 2013 Vincent Sanders <vince@netsurf-browser.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
#   * The above copyright notice and this permission notice shall be included in
#     all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# NetSurf continuius integration build script for jenkins
#
# This script may be executed by jenkins jobs that use the core buildsystem
#
# Usage: jenkins-build.sh [install|test-install|coverage|static]
#
# install - build and install
# test-install - build, test and install
# coverage - run coverage
# static - perform a static analysis
# coverity - perform a coverity scan

# TARGET must be in the environment and set correctly
if [ "x${TARGET}" = "x" ];then
    echo "TARGET unset"
    exit 1
fi

# The target for built artifacts
#
# This does mean the artifacts of a target must all be built on the
#   same jenkins slave instance
ARTIFACT_HOME=${JENKINS_HOME}/artifacts-${TARGET}

# Obtain the native target
NATIVE_TARGET=$(uname -s)

# target defaults
TARGET_TEST=
TARGET_INSTALL=
TARGET_COVERAGE=
TARGET_STATIC=
TARGET_COVERITY=
TARGET_BUILD="release"

# change defaults based on build parameter
case "$1" in
    "install")
	TARGET_INSTALL=${TARGET}
	;;

    "coverage")
	TARGET_COVERAGE=${TARGET}
	TARGET_BUILD="debug"
	# need to disable ccache on coverage builds
	export CCACHE=
	;;

    "static")
	TARGET_STATIC=${TARGET}
	TARGET_BUILD="debug"
	# need to disable ccache on static builds
	export CCACHE=
	;;

    "coverity")
	TARGET_COVERITY=${TARGET}
	TARGET_BUILD="debug"
	# need to disable ccache on coverity builds
	export CCACHE=
	;;

    "test-install")
	# Perfom test if being executed on native target
	TARGET_TEST=${NATIVE_TARGET}
	TARGET_INSTALL=${TARGET}
	;;

    "")
	# default is test only on Linux and install
	# Currently most tests do not work on targets except for Linux
	# TARGET_TEST=${NATIVE_TARGET}
	TARGET_TEST="Linux"
	TARGET_INSTALL=${TARGET}
	;;

    *)
	cat <<EOF
Usage: jenkins-build.sh [install|test-install|coverage|static]

       install        build and install
       test-install   build, test and install
       coverage       run coverage
       static         perform a static anaysis
       coverity       perform a coverity scan
EOF
	exit 1
	;;
esac

# currently core buildsystem doesnt use triplets so we need to adjust for that
# here is also where we can adjust other setting based on target
case ${TARGET} in
    "m68k-atari-mint")
	TARGET_TARGET="atari"
	;;

    "m5475-atari-mint")
	TARGET_TARGET="m5475_atari"
	;;

    "powerpc-apple-darwin9")
	TARGET_TARGET="powerpc_apple_darwin9"
	;;

    *)
	TARGET_TARGET=${TARGET}
	;;
esac

# Ensure the artifact target directory exists
mkdir -p ${ARTIFACT_HOME}

# Configure all build paths relative to prefix
export PREFIX=${ARTIFACT_HOME}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin

# execute the build steps

# clean target is always first
make Q= clean TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD}

# build as per type requested
if [ "x${TARGET}" = "x${TARGET_COVERAGE}" ]; then
    # Coverage Build
    make Q= TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD} coverage
    gcovr -x -r .. -o coverage.xml

elif [ "x${TARGET}" = "x${TARGET_STATIC}" ]; then
    # static build
    rm -rf clangScanBuildReports

    scan-build -o clangScanBuildReports -v --use-cc clang --use-analyzer=/usr/bin/clang make BUILD=debug

    # clean up after
    make Q= clean TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD}

elif [ "x${TARGET}" = "x${TARGET_COVERITY}" ]; then
    # coverity build

    # Check thses are set
    #
    # COVERITY_PROJECT
    # COVERITY_TOKEN
    # COVERITY_USER
    # COVERITY_PREFIX

    if [ -z "${COVERITY_PROJECT}" -o -z "${COVERITY_TOKEN}" -o -z "${COVERITY_USER}" -o -z "${COVERITY_PREFIX}" ]; then
	echo "Coverity parameters not set"
	exit 1
    fi

    # Coverity tools location
    COVERITY_PREFIX=${COVERITY_PREFIX:-/opt/coverity/cov-analysis-linux64-6.6.1}
    COVERITY_VERSION=$(git rev-parse HEAD)


    export PATH=${PATH}:${COVERITY_PREFIX}/bin

    # cleanup before we start
    rm -rf cov-int/ coverity-scan.tar.gz coverity-scan.tar

    cov-build --dir cov-int make Q= TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD}

    tar cf coverity-scan.tar cov-int

    gzip -9 coverity-scan.tar

    curl --form "project=${COVERITY_PROJECT}" --form "token=${COVERITY_TOKEN}" --form "email=${COVERITY_USER}" --form "file=@coverity-scan.tar.gz" --form "version=${COVERITY_VERSION}" --form "description=Git Head build" http://scan5.coverity.com/cgi-bin/upload.py

else
    # Normal build
    make Q= TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD}

fi

# test if appropriate
if [ "x${TARGET}" = "x${TARGET_TEST}" ]; then
    make Q= TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD} test
fi

# install
if [ "x${TARGET}" = "x${TARGET_INSTALL}" ]; then
    make Q= TARGET=${TARGET_TARGET} BUILD=${TARGET_BUILD} install
fi
