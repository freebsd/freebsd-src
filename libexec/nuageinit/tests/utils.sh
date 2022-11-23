atf_test_case warn
atf_test_case err
atf_test_case dirname

warn_body() {
	atf_check -e "inline:plop\n" -s exit:0 /usr/libexec/flua $(atf_get_srcdir)/warn.lua
}

err_body() {
	atf_check -e "inline:plop\n" -s exit:1 /usr/libexec/flua $(atf_get_srcdir)/err.lua
}

dirname_body() {
	atf_check -o "inline:/my/path/\n" -s exit:0 /usr/libexec/flua $(atf_get_srcdir)/dirname.lua
}

atf_init_test_cases() {
	atf_add_test_case warn
	atf_add_test_case err
	atf_add_test_case dirname
}
