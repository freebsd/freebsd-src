#!/bin/bash
#################################################################################
# Copyright 2018 ARM. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without modification, 
# are permitted provided that the following conditions are met:
# 
# 1. Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer.
# 
# 2. Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
# 
# 3. Neither the name of the copyright holder nor the names of its contributors 
# may be used to endorse or promote products derived from this software without 
# specific prior written permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 'AS IS' AND 
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, 
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES 
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND 
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
# 
#################################################################################
# OpenCSD library: run single test
#
#
#################################################################################
# Usage options:-
# * default: run test on binary + libs in ./bin/linux64/rel
# run_pkt_decode_tests.bash <test> <options>
#
# * use installed opencsd libraries & program
# run_pkt_decode_tests.bash use-installed <test> <options>
#
#

OUT_DIR=./results
SNAPSHOT_DIR=./snapshots
BIN_DIR=./bin/linux64/rel/

TEST="a57_single_step"

mkdir -p ${OUT_DIR}

if [ "$1" == "use-installed" ]; then
    BIN_DIR=""
    shift
fi

if [ "$1" != "" ]; then
    TEST=$1
    shift
fi

echo "Running trc_pkt_lister on single snapshot ${TEST}"


if [ "${BIN_DIR}" != "" ]; then
    echo "Tests using BIN_DIR = ${BIN_DIR}"
    export LD_LIBRARY_PATH=${BIN_DIR}.
    echo "LD_LIBRARY_PATH set to ${BIN_DIR}"
else
    echo "Tests using installed binaries"
fi

# === test the decode set ===
${BIN_DIR}trc_pkt_lister -ss_dir "${SNAPSHOT_DIR}/${TEST}" $@ -decode -logfilename "${OUT_DIR}/${TEST}.ppl"
echo "Done : Return $?"


