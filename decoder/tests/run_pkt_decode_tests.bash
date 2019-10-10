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
# OpenCSD library: Test script.
#
# Test script to run packet lister on each of the snapshots retained with the repository.
# No attempt is made to compare output results to previous versions,  (output formatting
# may change due to bugfix / enhancements) or assess the validity  of the trace output.
#
#################################################################################

OUT_DIR=./results
SNAPSHOT_DIR=./snapshots
BIN_DIR=./bin/linux64/rel

# directories for tests using full decode
declare -a test_dirs_decode=( "juno-ret-stck"
                              "a57_single_step"
                              "bugfix-exact-match"
                              "juno-uname-001"
                              "juno-uname-002"
                              "juno_r1_1"
                              "tc2-ptm-rstk-t32"
                              "trace_cov_a15"
                              "stm_only"
                              "stm_only-2"
                              "stm_only-juno"
                              "TC2"
                              "Snowball"
                              "test-file-mem-offsets"
                            )


echo "Running trc_pkt_lister on snapshot directories."

mkdir -p ${OUT_DIR}

# === test the decode set ===
export LD_LIBRARY_PATH=${BIN_DIR}/.

for test_dir in "${test_dirs_decode[@]}"
do
    echo "Testing $test_dir..."
    ${BIN_DIR}/trc_pkt_lister -ss_dir "${SNAPSHOT_DIR}/$test_dir" -decode -logfilename "${OUT_DIR}/$test_dir.ppl"
    echo "Done : Return $?"
done

# === test the TPIU deformatter ===
echo "Testing a55-test-tpiu..."
${BIN_DIR}/trc_pkt_lister -ss_dir "${SNAPSHOT_DIR}/a55-test-tpiu" -dstream_format -o_raw_packed -o_raw_unpacked -logfilename "${OUT_DIR}/a55-test-tpiu.ppl"
echo "Done : Return $?"
