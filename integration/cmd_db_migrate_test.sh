# Copyright 2013 The Kyua Authors.
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


# Location of installed schema files.
: "${KYUA_STOREDIR:=__KYUA_STOREDIR__}"


# Location of installed test data files.
: "${KYUA_STORETESTDATADIR:=__KYUA_STORETESTDATADIR__}"


# Creates an empty old-style action database.
#
# \param ... Files that contain SQL commands to be run.
create_historical_db() {
    mkdir -p "${HOME}/.kyua"
    cat "${@}" | sqlite3 "${HOME}/.kyua/store.db"
}


# Creates an empty results file.
#
# \param ... Files that contain SQL commands to be run.
create_results_file() {
    mkdir -p "${HOME}/.kyua/store"
    local dbname="results.$(utils_test_suite_id)-20140718-173200-123456.db"
    cat "${@}" | sqlite3 "${HOME}/.kyua/store/${dbname}"
}


utils_test_case upgrade__from_v1
upgrade__from_v1_head() {
    atf_set require.files \
        "${KYUA_STORETESTDATADIR}/schema_v1.sql" \
        "${KYUA_STORETESTDATADIR}/testdata_v1.sql" \
        "${KYUA_STOREDIR}/migrate_v1_v2.sql" \
        "${KYUA_STOREDIR}/migrate_v2_v3.sql"
    atf_set require.progs "sqlite3"
}
upgrade__from_v1_body() {
    create_historical_db "${KYUA_STORETESTDATADIR}/schema_v1.sql" \
        "${KYUA_STORETESTDATADIR}/testdata_v1.sql"
    atf_check -s exit:0 -o empty -e empty kyua db-migrate
    for f in \
        "results.test_suite_root.20130108-111331-000000.db" \
        "results.usr_tests.20130108-123832-000000.db" \
        "results.usr_tests.20130108-112635-000000.db"
    do
        [ -f "${HOME}/.kyua/store/${f}" ] || atf_fail "Expected file ${f}" \
            "was not created"
    done
    [ ! -f "${HOME}/.kyua/store.db" ] || atf_fail "Historical database not" \
        "deleted"
}


utils_test_case upgrade__from_v2
upgrade__from_v2_head() {
    atf_set require.files \
        "${KYUA_STORETESTDATADIR}/schema_v2.sql" \
        "${KYUA_STORETESTDATADIR}/testdata_v2.sql" \
        "${KYUA_STOREDIR}/migrate_v2_v3.sql"
    atf_set require.progs "sqlite3"
}
upgrade__from_v2_body() {
    create_historical_db "${KYUA_STORETESTDATADIR}/schema_v2.sql" \
        "${KYUA_STORETESTDATADIR}/testdata_v2.sql"
    atf_check -s exit:0 -o empty -e empty kyua db-migrate
    for f in \
        "results.test_suite_root.20130108-111331-000000.db" \
        "results.usr_tests.20130108-123832-000000.db" \
        "results.usr_tests.20130108-112635-000000.db"
    do
        [ -f "${HOME}/.kyua/store/${f}" ] || atf_fail "Expected file ${f}" \
            "was not created"
    done
    [ ! -f "${HOME}/.kyua/store.db" ] || atf_fail "Historical database not" \
        "deleted"
}


utils_test_case already_up_to_date
already_up_to_date_head() {
    atf_set require.files "${KYUA_STOREDIR}/schema_v3.sql"
    atf_set require.progs "sqlite3"
}
already_up_to_date_body() {
    create_results_file "${KYUA_STOREDIR}/schema_v3.sql"
    atf_check -s exit:1 -o empty -e match:"already at schema version" \
        kyua db-migrate
}


utils_test_case need_upgrade
need_upgrade_head() {
    atf_set require.files "${KYUA_STORETESTDATADIR}/schema_v1.sql"
    atf_set require.progs "sqlite3"
}
need_upgrade_body() {
    create_results_file "${KYUA_STORETESTDATADIR}/schema_v1.sql"
    atf_check -s exit:2 -o empty \
        -e match:"database has schema version 1.*use db-migrate" kyua report
}


utils_test_case results_file__ok
results_file__ok_body() {
    echo "This is not a valid database" >test.db
    atf_check -s exit:1 -o empty -e match:"Migration failed" \
        kyua db-migrate --results-file ./test.db
}


utils_test_case results_file__fail
results_file__fail_body() {
    atf_check -s exit:1 -o empty -e match:"No previous results.*test.db" \
        kyua db-migrate --results-file ./test.db
}


utils_test_case too_many_arguments
too_many_arguments_body() {
    cat >stderr <<EOF
Usage error for command db-migrate: Too many arguments.
Type 'kyua help db-migrate' for usage information.
EOF
    atf_check -s exit:3 -o empty -e file:stderr kyua db-migrate abc def
}


atf_init_test_cases() {
    atf_add_test_case upgrade__from_v1
    atf_add_test_case upgrade__from_v2
    atf_add_test_case already_up_to_date
    atf_add_test_case need_upgrade

    atf_add_test_case results_file__ok
    atf_add_test_case results_file__fail

    atf_add_test_case too_many_arguments
}
