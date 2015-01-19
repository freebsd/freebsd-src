# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin

# Test add user
atf_test_case user_add
user_add_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test
	atf_check -s exit:0 -o match:"^test:.*" \
		grep "^test:.*" $HOME/master.passwd
}

# Test add user with option -N
atf_test_case user_add_noupdate
user_add_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:0 -o match:"^test:.*" ${PW} useradd test -N
	atf_check -s exit:1 -o empty grep "^test:.*" $HOME/master.passwd
}

# Test add user with comments
atf_test_case user_add_comments
user_add_comments_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test -c "Test User,work,123,456"
	atf_check -s exit:0 -o match:"^test:.*:Test User,work,123,456:" \
		grep "^test:.*:Test User,work,123,456:" $HOME/master.passwd
}

# Test add user with comments and option -N
atf_test_case user_add_comments_noupdate
user_add_comments_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:0 -o match:"^test:.*:Test User,work,123,456:" \
		${PW} useradd test -c "Test User,work,123,456" -N
	atf_check -s exit:1 -o empty grep "^test:.*" $HOME/master.passwd
}

# Test add user with invalid comments
atf_test_case user_add_comments_invalid
user_add_comments_invalid_body() {
	populate_etc_skel

	atf_check -s exit:65 -e match:"invalid character" \
		${PW} useradd test -c "Test User,work,123:456,456"
	atf_check -s exit:1 -o empty \
		grep "^test:.*:Test User,work,123:456,456:" $HOME/master.passwd
}

# Test add user with invalid comments and option -N
atf_test_case user_add_comments_invalid_noupdate
user_add_comments_invalid_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:65 -e match:"invalid character" \
		${PW} useradd test -c "Test User,work,123:456,456" -N
	atf_check -s exit:1 -o empty grep "^test:.*" $HOME/master.passwd
}

atf_init_test_cases() {
	atf_add_test_case user_add
	atf_add_test_case user_add_noupdate
	atf_add_test_case user_add_comments
	atf_add_test_case user_add_comments_noupdate
	atf_add_test_case user_add_comments_invalid 
	atf_add_test_case user_add_comments_invalid_noupdate
}
