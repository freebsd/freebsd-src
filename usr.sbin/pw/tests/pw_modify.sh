# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin


# Test adding & removing a user from a group
atf_test_case groupmod_user
groupmod_user_body() {
	populate_etc_skel
	atf_check -s exit:0 pw -V ${HOME} addgroup test
	atf_check -s exit:0 pw -V ${HOME} groupmod test -m root
	atf_check -s exit:0 -o match:"^test:\*:1001:root$" \
		grep "^test:\*:.*:root$" $HOME/group
	atf_check -s exit:0 pw -V ${HOME} groupmod test -d root
	atf_check -s exit:0 -o match:"^test:\*:1001:$" \
		grep "^test:\*:.*:$" $HOME/group
}


# Test adding and removing a user that does not exist
atf_test_case groupmod_invalid_user
groupmod_invalid_user_body() {
	populate_etc_skel
	atf_check -s exit:0 pw -V ${HOME} addgroup test
	atf_check -s exit:67 -e match:"does not exist" pw -V ${HOME} groupmod test -m foo
	atf_check -s exit:0  pw -V ${HOME} groupmod test -d foo
}

atf_test_case groupmod_bug_193704
groupmod_bug_193704_head() {
	atf_set "descr" "Regression test for the #193704 bug"
}
groupmod_bug_193704_body() {
	populate_etc_skel
	atf_check -s exit:0 -x pw -V ${HOME} groupadd test
	atf_check -s exit:0 -x pw -V ${HOME} groupmod test -l newgroupname
	atf_check -s exit:65 -e match:"^pw: unknown group" -x pw -V ${HOME} groupshow test
}

atf_init_test_cases() {
	atf_add_test_case groupmod_user
	atf_add_test_case groupmod_invalid_user
	atf_add_test_case groupmod_bug_193704
}
