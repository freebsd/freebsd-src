# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin

# Test locking and unlocking a user account
atf_test_case user_locking cleanup
user_locking_body() {
	populate_etc_skel
	${PW} useradd test || atf_fail "Creating test user"
	${PW} lock test || atf_fail "Locking the user"
	atf_check -s exit:0 -o match:"^test:\*LOCKED\*\*:1001:" \
		grep "^test:\*LOCKED\*\*:1001:" $HOME/master.passwd
	${PW} unlock test || atf_fail "Locking the user"
	atf_check -s exit:0 -o match:"^test:\*:1001:" \
		grep "^test:\*:1001:" $HOME/master.passwd
}


atf_init_test_cases() {
	atf_add_test_case user_locking
}
