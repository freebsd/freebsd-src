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
	pw -V ${HOME} useradd test || atf_fail "Creating test user"
	pw -V ${HOME} groupmod test -M 'test,root' || \
		atf_fail "Modifying the group"
	pw -V ${HOME} userdel test || atf_fail "delete the user"
}


atf_init_test_cases() {
	atf_add_test_case rmuser_seperate_group
}
