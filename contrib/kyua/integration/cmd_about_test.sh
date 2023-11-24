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


# Location of installed documents.  Used to validate the output of the about
# messages against the golden files.
: "${KYUA_DOCDIR:=__KYUA_DOCDIR__}"


# Common code to validate the output of all about information.
#
# \param file The name of the file with the output.
check_all() {
    local file="${1}"; shift

    grep -E 'kyua .*[0-9]+\.[0-9]+' "${file}" || \
        atf_fail 'No version reported'
    grep 'Copyright' "${file}" || atf_fail 'No license reported'
    grep '^\*[^<>]*$' "${file}" || atf_fail 'No authors reported'
    grep '^\*.*<.*@.*>$' "${file}" || atf_fail 'No contributors reported'
    grep 'Homepage' "${file}" || atf_fail 'No homepage reported'
}


utils_test_case all_topics__installed
all_topics__installed_head() {
    atf_set "require.files" "${KYUA_DOCDIR}/AUTHORS" \
            "${KYUA_DOCDIR}/CONTRIBUTORS" "${KYUA_DOCDIR}/LICENSE"
}
all_topics__installed_body() {
    atf_check -s exit:0 -o save:stdout -e empty kyua about
    check_all stdout
}


utils_test_case all_topics__override
all_topics__override_body() {
    mkdir docs
    echo "* Author (no email)" >docs/AUTHORS
    echo "* Contributor <contributor@example.net>" >docs/CONTRIBUTORS
    echo "Copyright text" >docs/LICENSE
    export KYUA_DOCDIR=docs
    atf_check -s exit:0 -o save:stdout -e empty kyua about
    check_all stdout
}


utils_test_case topic__authors__installed
topic__authors__installed_head() {
    atf_set "require.files" "${KYUA_DOCDIR}/AUTHORS" \
            "${KYUA_DOCDIR}/CONTRIBUTORS"
}
topic__authors__installed_body() {
    grep -h '^\* ' "${KYUA_DOCDIR}/AUTHORS" "${KYUA_DOCDIR}/CONTRIBUTORS" \
         >expout
    atf_check -s exit:0 -o file:expout -e empty kyua about authors
}


utils_test_case topic__authors__override
topic__authors__override_body() {
    mkdir docs
    echo "* Author (no email)" >docs/AUTHORS
    echo "* Contributor <contributor@example.net>" >docs/CONTRIBUTORS
    export KYUA_DOCDIR=docs
    cat docs/AUTHORS docs/CONTRIBUTORS >expout
    atf_check -s exit:0 -o file:expout -e empty kyua about authors
}


utils_test_case topic__license__installed
topic__license__installed_head() {
    atf_set "require.files" "${KYUA_DOCDIR}/LICENSE"
}
topic__license__installed_body() {
    atf_check -s exit:0 -o file:"${KYUA_DOCDIR}/LICENSE" -e empty \
        kyua about license
}


utils_test_case topic__license__override
topic__license__override_body() {
    mkdir docs
    echo "Copyright text" >docs/LICENSE
    export KYUA_DOCDIR=docs
    atf_check -s exit:0 -o file:docs/LICENSE -e empty kyua about license
}


utils_test_case topic__version
topic__version_body() {
    atf_check -s exit:0 -o save:stdout -e empty kyua about version

    local lines="$(wc -l stdout | awk '{ print $1 }')"
    [ "${lines}" -eq 1 ] || atf_fail "Version query returned more than one line"

    grep -E '^kyua (.*) [0-9]+\.[0-9]+$' stdout || \
        atf_fail "Invalid version message"
}


utils_test_case topic__invalid
topic__invalid_body() {
    cat >experr <<EOF
Usage error for command about: Invalid about topic 'foo'.
Type 'kyua help about' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:experr kyua about foo
}


utils_test_case too_many_arguments
too_many_arguments_body() {
    cat >stderr <<EOF
Usage error for command about: Too many arguments.
Type 'kyua help about' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:stderr kyua about abc def
}


atf_init_test_cases() {
    atf_add_test_case all_topics__installed
    atf_add_test_case all_topics__override
    atf_add_test_case topic__authors__installed
    atf_add_test_case topic__authors__override
    atf_add_test_case topic__license__installed
    atf_add_test_case topic__license__override
    atf_add_test_case topic__version
    atf_add_test_case topic__invalid

    atf_add_test_case too_many_arguments
}
