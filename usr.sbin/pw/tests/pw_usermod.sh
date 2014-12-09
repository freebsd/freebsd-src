# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin

# Test modifying a user
atf_test_case user_mod
user_mod_body() {
	populate_etc_skel

	atf_check -s exit:67 -e match:"no such user" ${PW} usermod test
	atf_check -s exit:0 ${PW} useradd test
	atf_check -s exit:0 ${PW} usermod test
	atf_check -s exit:0 -o match:"^test:.*" \
		grep "^test:.*" $HOME/master.passwd
}

# Test modifying a user with comments
atf_test_case user_mod_comments
user_mod_comments_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test -c "Test User,home,123,456"
	atf_check -s exit:0 ${PW} usermod test -c "Test User,work,123,456"
	atf_check -s exit:0 -o match:"^test:.*:Test User,work,123,456:" \
		grep "^test:.*:Test User,work,123,456:" $HOME/master.passwd
}

# Test modifying a user with invalid comments
atf_test_case user_mod_comments_invalid
user_mod_comments_invalid_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test
	atf_check -s exit:65 -e match:"invalid character" \
		${PW} usermod test -c "Test User,work,123:456,456"
	atf_check -s exit:1 -o empty \
		grep "^test:.*:Test User,work,123:456,456:" $HOME/master.passwd
}

# Test modifying a user name with -l
atf_test_case user_mod_name
user_mod_name_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd foo
	atf_check -s exit:0 ${PW} usermod foo -l "bar"
	atf_check -s exit:0 -o match:"^bar:.*" \
		grep "^bar:.*" $HOME/master.passwd
}
atf_init_test_cases() {
	atf_add_test_case user_mod
	atf_add_test_case user_mod_comments
	atf_add_test_case user_mod_comments_invalid 
	atf_add_test_case user_mod_name
}
