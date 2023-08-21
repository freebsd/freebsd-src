# Copyright 2011 The Kyua Authors.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
# * Neither the name of Google Inc. nor the names of its contributors
#   may be used to endorse or promote products derived from this software
#   without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


# Subcommand to strip out the durations and timestamps in a report.
#
# This is to make the reports deterministic and thus easily testable.  The
# time deltas are replaced by the fixed string S.UUU and the timestamps are
# replaced by the fixed strings YYYYMMDD.HHMMSS.ssssss and
# YYYY-MM-DDTHH:MM:SS.ssssssZ depending on their original format.
#
# This variable should be used as shown here:
#
#     atf_check ... -x kyua report "| ${utils_strip_times}"
#
# Use the utils_install_times_wrapper function to create a 'kyua' wrapper
# script that automatically does this.
# CHECK_STYLE_DISABLE
utils_strip_times='sed -E \
    -e "s,( |\[|\")[0-9][0-9]*\.[0-9][0-9][0-9](s]|s|\"),\1S.UUU\2,g" \
    -e "s,[0-9]{8}-[0-9]{6}-[0-9]{6},YYYYMMDD-HHMMSS-ssssss,g" \
    -e "s,[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{6}Z,YYYY-MM-DDTHH:MM:SS.ssssssZ,g"'
# CHECK_STYLE_ENABLE


# Same as utils_strip_times but avoids stripping timestamp-based report IDs.
#
# This is to make the reports deterministic and thus easily testable.  The
# time deltas are replaced by the fixed string S.UUU and the timestamps are
# replaced by the fixed string YYYY-MM-DDTHH:MM:SS.ssssssZ.
# CHECK_STYLE_DISABLE
utils_strip_times_but_not_ids='sed -E \
    -e "s,( |\[|\")[0-9][0-9]*\.[0-9][0-9][0-9](s]|s|\"),\1S.UUU\2,g" \
    -e "s,[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}\.[0-9]{6}Z,YYYY-MM-DDTHH:MM:SS.ssssssZ,g"'
# CHECK_STYLE_ENABLE


# Computes the results id for a test suite run.
#
# The computed path is "generic" in the sense that it does not include a
# real timestamp: it only includes a placeholder.  This function should be
# used along the utils_strip_times function so that the timestamps of
# the real results files are stripped out.
#
# \param path Optional path to use; if not given, use the cwd.
utils_results_id() {
    local test_suite_id="$(utils_test_suite_id "${@}")"
    echo "${test_suite_id}.YYYYMMDD-HHMMSS-ssssss"
}


# Computes the results file for a test suite run.
#
# The computed path is "generic" in the sense that it does not include a
# real timestamp: it only includes a placeholder.  This function should be
# used along the utils_strip_times function so that the timestampts of the
# real results files are stripped out.
#
# \param path Optional path to use; if not given, use the cwd.
utils_results_file() {
    echo "${HOME}/.kyua/store/results.$(utils_results_id "${@}").db"
}


# Copies a helper binary from the source directory to the work directory.
#
# \param name The name of the binary to copy.
# \param destination The target location for the binary; can be either
#     a directory name or a file name.
utils_cp_helper() {
    local name="${1}"; shift
    local destination="${1}"; shift

    ln -s "$(atf_get_srcdir)"/helpers/"${name}" "${destination}"
}


# Creates a 'kyua' binary in the path that strips timing data off the output.
#
# Call this on test cases that wish to replace timing data in the *stdout* of
# Kyua with the deterministic strings.  This is to be used by tests that
# validate the 'test' and 'report' subcommands.
utils_install_times_wrapper() {
    [ ! -x kyua ] || return
    cat >kyua <<EOF
#! /bin/sh

PATH=${PATH}

kyua "\${@}" >kyua.tmpout
result=\${?}
cat kyua.tmpout | ${utils_strip_times}
exit \${result}
EOF
    chmod +x kyua
    PATH="$(pwd):${PATH}"
}


# Creates a 'kyua' binary in the path that makes the output of 'test' stable.
#
# Call this on test cases that wish to replace timing data with deterministic
# strings and that need the result lines in the output to be sorted
# lexicographically.  The latter hides the indeterminism caused by parallel
# execution so that the output can be verified.  For these reasons, this is to
# be used exclusively by tests that validate the 'test' subcommand.
utils_install_stable_test_wrapper() {
    [ ! -x kyua ] || return
    cat >kyua <<EOF
#! /bin/sh

PATH=${PATH}

kyua "\${@}" >kyua.tmpout
result=\${?}
cat kyua.tmpout | ${utils_strip_times} >kyua.tmpout2

# Sort the test result lines but keep the rest intact.
grep '[^ ]*:[^ ]*' kyua.tmpout2 | sort >kyua.tmpout3
grep -v '[^ ]*:[^ ]*' kyua.tmpout2 >kyua.tmpout4
cat kyua.tmpout3 kyua.tmpout4

exit \${result}
EOF
    chmod +x kyua
    PATH="$(pwd):${PATH}"
}


# Defines a test case with a default head.
utils_test_case() {
    local name="${1}"; shift

    atf_test_case "${name}"
    eval "${name}_head() {
        atf_set require.progs kyua
    }"
}


# Computes the test suite identifier for results files files.
#
# \param path Optional path to use; if not given, use the cwd.
utils_test_suite_id() {
    local path=
    if [ ${#} -gt 0 ]; then
        path="$(cd ${1} && pwd)"; shift
    else
        path="$(pwd)"
    fi
    echo "${path}" | sed -e 's,^/,,' -e 's,/,_,g'
}
