# $FreeBSD$

# When provide -V dir, dir must exists
atf_test_case etcdir_must_exists
etcdir_must_exists_head() {
	atf_set "descr" "When provide -V dir, dir must exists"
}

etcdir_must_exists_body() {
	local fakedir="/this_directory_does_not_exists"
	atf_check -e inline:"pw: no such directory \`$fakedir'\n" \
		-s exit:72 -x pw -V ${fakedir} usershow root
}

atf_init_test_cases() {
	atf_add_test_case etcdir_must_exists
}

