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


# Creates a new database file in the store directory.
#
# Subsequent invocations of db-exec should just pick this new file up.
create_empty_store() {
    cat >Kyuafile <<EOF
syntax(2)
EOF
    atf_check -s exit:0 -o ignore -e empty kyua test
}


utils_test_case one_arg
one_arg_body() {
    create_empty_store

    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua db-exec "SELECT * FROM metadata"
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case many_args
many_args_body() {
    create_empty_store

    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua db-exec SELECT "*" FROM metadata
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case no_args
no_args_body() {
    atf_check -s exit:3 -o empty -e match:"Not enough arguments" kyua db-exec
    test ! -d .kyua/store/ || atf_fail "Database created but it should" \
        "not have been"
}


utils_test_case invalid_statement
invalid_statement_body() {
    create_empty_store

    atf_check -s exit:1 -o empty -e match:"SQLite error.*foo" \
        kyua db-exec foo
}


utils_test_case no_create_store
no_create_store_body() {
    atf_check -s exit:1 -o empty -e match:"No previous results.*not-here" \
        kyua db-exec --results-file=not-here "SELECT * FROM metadata"
    if [ -f not-here ]; then
        atf_fail "Database created but it should not have been"
    fi
}


utils_test_case results_file__default_home
results_file__default_home_body() {
    HOME=home-dir
    create_empty_store

    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua db-exec "SELECT * FROM metadata"
    test -f home-dir/.kyua/store/*.db || atf_fail "Database not created in" \
        "the home directory"
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case results_file__explicit__ok
results_file__explicit__ok_body() {
    create_empty_store
    mv .kyua/store/*.db custom.db
    rmdir .kyua/store

    HOME=home-dir
    atf_check -s exit:0 -o save:metadata.csv -e empty \
        kyua --logfile=/dev/null db-exec -r custom.db "SELECT * FROM metadata"
    test ! -d home-dir/.kyua || atf_fail "Home directory created but this" \
        "should not have happened"
    atf_check -s exit:0 -o ignore -e empty \
        grep 'schema_version,.*timestamp' metadata.csv
}


utils_test_case results_file__explicit__fail
results_file__explicit__fail_head() {
    atf_set "require.user" "unprivileged"
}
results_file__explicit__fail_body() {
    atf_check -s exit:1 -o empty -e match:"No previous results.*foo.db" \
        kyua db-exec --results-file=foo.db "SELECT * FROM metadata"
}


utils_test_case no_headers_flag
no_headers_flag_body() {
    create_empty_store

    atf_check kyua db-exec "CREATE TABLE data" \
        "(a INTEGER PRIMARY KEY, b INTEGER, c TEXT)"
    atf_check kyua db-exec "INSERT INTO data VALUES (65, 43, NULL)"
    atf_check kyua db-exec "INSERT INTO data VALUES (23, 42, 'foo')"

    cat >expout <<EOF
a,b,c
23,42,foo
65,43,NULL
EOF
    atf_check -s exit:0 -o file:expout -e empty \
        kyua db-exec "SELECT * FROM data ORDER BY a"

    tail -n 2 <expout >expout2
    atf_check -s exit:0 -o file:expout2 -e empty \
        kyua db-exec --no-headers "SELECT * FROM data ORDER BY a"
}


atf_init_test_cases() {
    atf_add_test_case one_arg
    atf_add_test_case many_args
    atf_add_test_case no_args
    atf_add_test_case invalid_statement
    atf_add_test_case no_create_store

    atf_add_test_case results_file__default_home
    atf_add_test_case results_file__explicit__ok
    atf_add_test_case results_file__explicit__fail

    atf_add_test_case no_headers_flag
}
