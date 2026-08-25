
# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin


# Test to make sure we do not accidentially delete wheel when trying to delete
# an unknown group
atf_test_case group_do_not_delete_wheel_if_group_unknown
group_do_not_delete_wheel_if_group_unknown_head() {
        atf_set "descr" "Make sure we do not consider gid 0 an unknown group"
}
group_do_not_delete_wheel_if_group_unknown_body() {
        populate_etc_skel
        atf_check -s exit:0 -o inline:"wheel:*:0:root\n" -x ${PW} groupshow wheel
        atf_check -e inline:"pw: Bad id 'I_do_not_exist': invalid\n" -s exit:64 -x \
		${PW} groupdel -g I_do_not_exist
        atf_check -s exit:0 -o inline:"wheel:*:0:root\n" -x ${PW} groupshow wheel
}


atf_test_case group_delete_by_gid cleanup
group_delete_by_gid_head() {
	atf_set "descr" "Test deleting a group by gid without providing a name"
}
group_delete_by_gid_body() {
	populate_etc_skel
	${PW} groupadd testgroup -g 1000 || atf_fail "Creating test group"
	atf_check -s exit:0 -o inline:"testgroup:*:1000:\n" \
		-x ${PW} groupshow 1000
	${PW} groupdel -g 1000 || atf_fail "Deleting group by gid"
}


atf_init_test_cases() {
	atf_add_test_case group_do_not_delete_wheel_if_group_unknown
	atf_add_test_case group_delete_by_gid
}
