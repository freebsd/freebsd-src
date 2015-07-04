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

# Test modifying a user with option -N
atf_test_case user_mod_noupdate
user_mod_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:67 -e match:"no such user" ${PW} usermod test -N
	atf_check -s exit:0 ${PW} useradd test
	atf_check -s exit:0 -o match:"^test:.*" ${PW} usermod test -N
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

# Test modifying a user with comments with option -N
atf_test_case user_mod_comments_noupdate
user_mod_comments_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test -c "Test User,home,123,456"
	atf_check -s exit:0 -o match:"^test:.*:Test User,work,123,456:" \
		${PW} usermod test -c "Test User,work,123,456" -N
	atf_check -s exit:0 -o match:"^test:.*:Test User,home,123,456:" \
		grep "^test:.*:Test User,home,123,456:" $HOME/master.passwd
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
	atf_check -s exit:0 -o match:"^test:\*" \
		grep "^test:\*" $HOME/master.passwd
}

# Test modifying a user with invalid comments with option -N
atf_test_case user_mod_comments_invalid_noupdate
user_mod_comments_invalid_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test
	atf_check -s exit:65 -e match:"invalid character" \
		${PW} usermod test -c "Test User,work,123:456,456" -N
	atf_check -s exit:1 -o empty \
		grep "^test:.*:Test User,work,123:456,456:" $HOME/master.passwd
	atf_check -s exit:0 -o match:"^test:\*" \
		grep "^test:\*" $HOME/master.passwd
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

# Test modifying a user name with -l with option -N
atf_test_case user_mod_name_noupdate
user_mod_name_noupdate_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd foo
	atf_check -s exit:0 -o match:"^bar:.*" ${PW} usermod foo -l "bar" -N
	atf_check -s exit:0 -o match:"^foo:.*" \
		grep "^foo:.*" $HOME/master.passwd
}

atf_test_case user_mod_rename
user_mod_rename_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd foo
	atf_check -s exit:0 ${PW} usermod foo -l bar
	atf_check -s exit:0 -o match:"^bar:.*" \
		grep "^bar:.*" ${HOME}/master.passwd
}

atf_test_case user_mod_rename_too_long
user_mod_rename_too_long_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd foo
	atf_check -s exit:64 -e match:"too long" ${PW} usermod foo \
		-l name_very_very_very_very_very_long
}

atf_test_case user_mod_h
user_mod_h_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd foo
	atf_check -s exit:0 ${PW} usermod foo -h 0 <<- EOF
	$(echo a)
	EOF
	atf_check -s exit:0 -o not-match:"^foo:\*:.*" \
		grep "^foo" ${HOME}/master.passwd
	atf_check -s exit:0 ${PW} usermod foo -h - <<- EOF
	$(echo b)
	EOF
	atf_check -s exit:0 -o match:"^foo:\*:.*" \
		grep "^foo" ${HOME}/master.passwd
	atf_check -e inline:"pw: '-h' expects a file descriptor or '-'\n" \
		-s exit:64 ${PW} usermod foo -h a <<- EOF
	$(echo a)
	EOF
}

atf_test_case user_mod_H
user_mod_H_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd foo
	atf_check -s exit:0 ${PW} usermod foo -H 0 <<- EOF
	$(echo a)
	EOF
	atf_check -s exit:0 -o match:"^foo:a:.*" \
		grep "^foo" ${HOME}/master.passwd
	atf_check -s exit:64 -e inline:"pw: '-H' expects a file descriptor\n" \
		${PW} usermod foo -H -
}

atf_init_test_cases() {
	atf_add_test_case user_mod
	atf_add_test_case user_mod_noupdate
	atf_add_test_case user_mod_comments
	atf_add_test_case user_mod_comments_noupdate
	atf_add_test_case user_mod_comments_invalid
	atf_add_test_case user_mod_comments_invalid_noupdate
	atf_add_test_case user_mod_rename
	atf_add_test_case user_mod_name_noupdate
	atf_add_test_case user_mod_rename
	atf_add_test_case user_mod_rename_too_long
	atf_add_test_case user_mod_h
	atf_add_test_case user_mod_H
}
