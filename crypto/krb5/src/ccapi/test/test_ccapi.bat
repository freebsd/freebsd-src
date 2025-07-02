# test_ccapi.bat

@echo "\nBeginning test of CCAPI...\n"
@echo "\nThese tests are based on the CCAPI v3 revision 8 draft documentation.\n"

#run_test simple_lock_test

teststest_constants.exe
tests\test_cc_initialize.exe
tests\test_cc_context_get_version.exe
exit 0

tests\test_cc_context_release.exe
tests\test_cc_context_get_change_time.exe
tests\test_cc_context_get_default_ccache_name.exe
tests\test_cc_context_open_ccache.exe
tests\test_cc_context_open_default_ccache.exe
tests\test_cc_context_create_ccache.exe
tests\test_cc_context_create_default_ccache.exe
tests\test_cc_context_create_new_ccache.exe
tests\test_cc_context_new_ccache_iterator.exe
tests\test_cc_context_compare.exe
tests\test_cc_ccache_release.exe
tests\test_cc_ccache_destroy.exe
tests\test_cc_ccache_set_default.exe
tests\test_cc_ccache_get_credentials_version.exe
tests\test_cc_ccache_get_name.exe
tests\test_cc_ccache_get_principal.exe
tests\test_cc_ccache_set_principal.exe
tests\test_cc_ccache_store_credentials.exe
tests\test_cc_ccache_remove_credentials.exe
tests\test_cc_ccache_new_credentials_iterator.exe
tests\test_cc_ccache_get_change_time.exe
tests\test_cc_ccache_get_last_default_time.exe
tests\test_cc_ccache_move.exe
tests\test_cc_ccache_compare.exe
tests\test_cc_ccache_get_kdc_time_offset.exe
tests\test_cc_ccache_set_kdc_time_offset.exe
tests\test_cc_ccache_clear_kdc_time_offset.exe
tests\test_cc_ccache_iterator_next.exe
tests\test_cc_credentials_iterator_next.exe

@echo "Finished testing CCAPI."
