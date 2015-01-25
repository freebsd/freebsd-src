# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin


# Test negative uid are still valid
# PR: 196514
atf_test_case show_user_with_negative_number
show_user_with_negative_number_body() {
	populate_etc_skel
	atf_check -s exit:0 \
		-o inline:"root:*:0:0::0:0:Charlie &:/root:/bin/csh\n" \
		${PW} usershow -n root -u -1
}

atf_init_test_cases() {
	atf_add_test_case show_user_with_negative_number
}
