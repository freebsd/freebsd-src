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
	atf_check -e inline:"pw: Bad id 'plop': invalid\n" -s exit:64 -x \
		${PW} userdel -u plop
}

atf_test_case delete_files
delete_files_body() {
	populate_root_etc_skel

	mkdir -p ${HOME}/skel
	touch ${HOME}/skel/a
	mkdir -p ${HOME}/home
	mkdir -p ${HOME}/var/mail
	atf_check -s exit:0 ${RPW} useradd foo -k /skel -m
	test -d ${HOME}/home || atf_fail "Fail to create home directory"
	test -f ${HOME}/var/mail/foo || atf_fail "Mail file not created"
	atf_check -s exit:0 ${RPW} userdel foo -r
	if test -f ${HOME}/var/mail/foo; then
		atf_fail "Mail file not removed"
	fi
}

atf_test_case delete_numeric_name
delete_numeric_name_body() {
	populate_etc_skel

	atf_check ${PW} useradd -n foo -u 4001
	atf_check -e inline:"pw: no such user \`4001'\n" -s exit:67 \
		${PW} userdel -n 4001
}

atf_test_case home_not_a_dir
home_not_a_dir_body() {
	populate_root_etc_skel
	touch ${HOME}/foo
	atf_check ${RPW} useradd foo -d /foo
	atf_check ${RPW} userdel foo -r
}

atf_test_case home_shared
home_shared_body() {
	populate_root_etc_skel
	mkdir ${HOME}/shared
	atf_check ${RPW} useradd -n testuser1 -d /shared
	atf_check ${RPW} useradd -n testuser2 -d /shared
	atf_check ${RPW} userdel -n testuser1 -r
	test -d ${HOME}/shared || atf_fail "Shared home has been removed"
}

atf_test_case home_regular_dir
home_regular_dir_body() {
	populate_root_etc_skel
	atf_check ${RPW} useradd -n foo -d /foo
	atf_check ${RPW} userdel -n foo -r
	[ ! -d ${HOME}/foo ] || atf_fail "Home has not been removed"
}

atf_init_test_cases() {
	atf_add_test_case rmuser_seperate_group
	atf_add_test_case user_do_not_try_to_delete_root_if_user_unknown
	atf_add_test_case delete_files
	atf_add_test_case delete_numeric_name
	atf_add_test_case home_not_a_dir
	atf_add_test_case home_shared
	atf_add_test_case home_regular_dir
}
