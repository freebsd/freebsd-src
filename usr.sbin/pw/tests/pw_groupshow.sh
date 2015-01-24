# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin


# Test negative uid are still valid
# PR: 196514
atf_test_case show_group_with_negative_number
show_group_with_negative_number_body() {
	populate_etc_skel
	atf_check -s exit:0 \
		-o inline:"wheel:*:0:root\n" \
		${PW} groupshow -n wheel -g -1
}

atf_init_test_cases() {
	atf_add_test_case show_group_with_negative_number
}
