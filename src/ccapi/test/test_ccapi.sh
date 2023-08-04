#!/bin/sh

# run with ./test-ccapi.sh to run CCAPI tests

TEST_DIR="tests"
failure_count=0

function run_test {
	if [[ -e $TEST_DIR/$1 ]]; then
		./$TEST_DIR/$1
		failure_count=`expr $failure_count + $?`
	fi
}

printf "\nBeginning test of CCAPI...\n"
printf "\nThese tests are based on the CCAPI v3 revision 8 draft documentation.\n"

run_test simple_lock_test

run_test test_constants

run_test test_cc_initialize
run_test test_cc_context_release
run_test test_cc_context_get_change_time
run_test test_cc_context_get_default_ccache_name
run_test test_cc_context_open_ccache
run_test test_cc_context_open_default_ccache
run_test test_cc_context_create_ccache
run_test test_cc_context_create_default_ccache
run_test test_cc_context_create_new_ccache
run_test test_cc_context_new_ccache_iterator
run_test test_cc_context_compare

run_test test_cc_ccache_release
run_test test_cc_ccache_destroy
run_test test_cc_ccache_set_default
run_test test_cc_ccache_get_credentials_version
run_test test_cc_ccache_get_name
run_test test_cc_ccache_get_principal
run_test test_cc_ccache_set_principal
run_test test_cc_ccache_store_credentials
run_test test_cc_ccache_remove_credentials
run_test test_cc_ccache_new_credentials_iterator
run_test test_cc_ccache_get_change_time
run_test test_cc_ccache_get_last_default_time
run_test test_cc_ccache_move
run_test test_cc_ccache_compare
run_test test_cc_ccache_get_kdc_time_offset
run_test test_cc_ccache_set_kdc_time_offset
run_test test_cc_ccache_clear_kdc_time_offset

run_test test_cc_ccache_iterator_next

run_test test_cc_credentials_iterator_next

run_test test_cc_shutdown
run_test test_cc_get_change_time
run_test test_cc_open
run_test test_cc_create
run_test test_cc_close
run_test test_cc_destroy
run_test test_cc_get_cred_version
run_test test_cc_get_name
run_test test_cc_get_principal
run_test test_cc_set_principal
run_test test_cc_store
run_test test_cc_remove_cred
run_test test_cc_seq_fetch_NCs_begin
run_test test_cc_seq_fetch_NCs_next
run_test test_cc_seq_fetch_creds_begin
run_test test_cc_seq_fetch_creds_next
run_test test_cc_get_NC_info

printf "\nFinished testing CCAPI. $failure_count failures in total.\n"

exit 0