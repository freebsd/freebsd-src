# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin


# Test that a user can be deleted when another user is part of this
# user's default group and does not go into an infinate loop.
# PR: 191427
atf_test_case rmuser_seperate_group cleanup
rmuser_seperate_group_head() {
	atf_set "timeout" "30"
}
rmuser_seperate_group_body() {
	populate_etc_skel
	${PW} useradd test || atf_fail "Creating test user"
	${PW} groupmod test -M 'test,root' || \
		atf_fail "Modifying the group"
	${PW} userdel test || atf_fail "Delete the test user"
}


atf_test_case user_do_not_try_to_delete_root_if_user_unknown
user_do_not_try_to_delete_root_if_user_unknown_head() {
	atf_set "descr" \
		"Make sure not to try to remove root if deleting an unknown user"
}
user_do_not_try_to_delete_root_if_user_unknown_body() {
	populate_etc_skel
	atf_check -e inline:"pw: -u expects a number\n" -s exit:64 -x \
		${PW} userdel -u plop
}

atf_init_test_cases() {
	atf_add_test_case rmuser_seperate_group
	atf_add_test_case user_do_not_try_to_delete_root_if_user_unknown
}
