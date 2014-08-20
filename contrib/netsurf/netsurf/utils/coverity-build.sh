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

# CI system coverity build and submission script
#
# Usage: coverity-build.sh
#

# environment variables
#
# COVERITY_TOKEN
# COVERITY_USER
# COVERITY_PREFIX path to tools else default is used
#
# either PREFIX or JENKINS_HOME

# build gtk, framebuffer and monkey frontend by default
TARGETS="gtk framebuffer monkey"

# We build for the Linux platform by default
ARTIFACT_TARGET=${TARGET:-Linux}

# setup build environment
export PREFIX=${PREFIX:-${JENKINS_HOME}/artifacts-${ARTIFACT_TARGET}}
export PKG_CONFIG_PATH=${PREFIX}/lib/pkgconfig
export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:${PREFIX}/lib
export PATH=${PATH}:${PREFIX}/bin

# Coverity tools location 
COVERITY_PREFIX=${COVERITY_PREFIX:-/opt/coverity/cov-analysis-linux64-6.6.1}
COVERITY_VERSION=$(git rev-parse HEAD)

export PATH=${PATH}:${COVERITY_PREFIX}/bin

# cleanup before we start
rm -rf cov-int/ covns.tar.gz covns.tar

for TARGET in ${TARGETS}; do
  make clean TARGET=${TARGET}
done

# Do the builds using coverity data gathering tool
for TARGET in ${TARGETS}; do
    cov-build --dir cov-int make CCACHE= TARGET=${TARGET}
done

tar cf covns.tar cov-int

gzip -9 covns.tar

curl --form "project=NetSurf Browser" --form "token=${COVERITY_TOKEN}" --form "email=${COVERITY_USER}" --form "file=@covns.tar.gz" --form "version=${COVERITY_VERSION}" --form "description=Git Head build" http://scan5.coverity.com/cgi-bin/upload.py