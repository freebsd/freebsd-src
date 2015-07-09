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

# Test add user with alternate homedir
atf_test_case user_add_homedir
user_add_homedir_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test -d /foo/bar
	atf_check -s exit:0 -o match:"^test:\*:.*::0:0:User &:/foo/bar:.*" \
		${PW} usershow test
}

# Test add user with account expiration as an epoch date
atf_test_case user_add_account_expiration_epoch
user_add_account_expiration_epoch_body() {
	populate_etc_skel

	DATE=`date -j -v+1d "+%s"`
	atf_check -s exit:0 ${PW} useradd test -e ${DATE}
	atf_check -s exit:0 -o match:"^test:\*:.*::0:${DATE}:.*" \
		${PW} usershow test
}

# Test add user with account expiration as a DD-MM-YYYY date
atf_test_case user_add_account_expiration_date_numeric
user_add_account_expiration_date_numeric_body() {
	populate_etc_skel

	DATE=`date -j -v+1d "+%d-%m-%Y"`
	EPOCH=`date -j -f "%d-%m-%Y %H:%M:%S" "${DATE} 00:00:00" "+%s"`
	atf_check -s exit:0 ${PW} useradd test -e ${DATE}
	atf_check -s exit:0 -o match:"^test:\*:.*::0:${EPOCH}:User &:.*" \
		${PW} usershow test
}

# Test add user with account expiration as a DD-MM-YYYY date
atf_test_case user_add_account_expiration_date_month
user_add_account_expiration_date_month_body() {
	populate_etc_skel

	DATE=`date -j -v+1d "+%d-%b-%Y"`
	EPOCH=`date -j -f "%d-%b-%Y %H:%M:%S" "${DATE} 00:00:00" "+%s"`
	atf_check -s exit:0 ${PW} useradd test -e ${DATE}
	atf_check -s exit:0 -o match:"^test:\*:.*::0:${EPOCH}:User &:.*" \
		${PW} usershow test
}

# Test add user with account expiration as a relative date
atf_test_case user_add_account_expiration_date_relative
user_add_account_expiration_date_relative_body() {
	populate_etc_skel

	EPOCH=`date -j -v+13m "+%s"`
	BUF=`expr $EPOCH + 5`
	atf_check -s exit:0 ${PW} useradd test -e +13o
	TIME=`${PW} usershow test | awk -F ':' '{print $7}'`
	[ ! -z $TIME -a $TIME -ge $EPOCH -a $TIME -lt $BUF ] || \
		atf_fail "Expiration time($TIME) was not within $EPOCH - $BUF seconds."
}

# Test add user with password expiration as an epoch date
atf_test_case user_add_password_expiration_epoch
user_add_password_expiration_epoch_body() {
	populate_etc_skel

	DATE=`date -j -v+1d "+%s"`
	atf_check -s exit:0 ${PW} useradd test -p ${DATE}
	atf_check -s exit:0 -o match:"^test:\*:.*::${DATE}:0:.*" \
		${PW} usershow test
}

# Test add user with password expiration as a DD-MM-YYYY date
atf_test_case user_add_password_expiration_date_numeric
user_add_password_expiration_date_numeric_body() {
	populate_etc_skel

	DATE=`date -j -v+1d "+%d-%m-%Y"`
	EPOCH=`date -j -f "%d-%m-%Y %H:%M:%S" "${DATE} 00:00:00" "+%s"`
	atf_check -s exit:0 ${PW} useradd test -p ${DATE}
	atf_check -s exit:0 -o match:"^test:\*:.*::${EPOCH}:0:User &:.*" \
		${PW} usershow test
}

# Test add user with password expiration as a DD-MMM-YYYY date
atf_test_case user_add_password_expiration_date_month
user_add_password_expiration_date_month_body() {
	populate_etc_skel

	DATE=`date -j -v+1d "+%d-%b-%Y"`
	EPOCH=`date -j -f "%d-%b-%Y %H:%M:%S" "${DATE} 00:00:00" "+%s"`
	atf_check -s exit:0 ${PW} useradd test -p ${DATE}
	atf_check -s exit:0 -o match:"^test:\*:.*::${EPOCH}:0:User &:.*" \
		${PW} usershow test
}

# Test add user with password expiration as a relative date
atf_test_case user_add_password_expiration_date_relative
user_add_password_expiration_date_relative_body() {
	populate_etc_skel

	EPOCH=`date -j -v+13m "+%s"`
	BUF=`expr $EPOCH + 5`
	atf_check -s exit:0 ${PW} useradd test -p +13o
	TIME=`${PW} usershow test | awk -F ':' '{print $6}'`
	[ ! -z $TIME -a $TIME -ge $EPOCH -a $TIME -lt $BUF ] || \
		atf_fail "Expiration time($TIME) was not within $EPOCH - $BUF seconds."
}

atf_test_case user_add_name_too_long
user_add_name_too_long_body() {
	populate_etc_skel
	atf_check -e match:"too long" -s exit:64 \
		${PW} useradd name_very_vert_very_very_very_long
}

atf_test_case user_add_expiration
user_add_expiration_body() {
	populate_etc_skel

	atf_check -s exit:0 \
		${PW} useradd foo -e 20-03-2043
	atf_check -o inline:"foo:*:1001:1001::0:2310422400:User &:/home/foo:/bin/sh\n" \
		-s exit:0 grep "^foo" ${HOME}/master.passwd
	atf_check -s exit:0 ${PW} userdel foo
	atf_check -s exit:0 \
		${PW} useradd foo -e 20-03-43
	atf_check -o inline:"foo:*:1001:1001::0:2310422400:User &:/home/foo:/bin/sh\n" \
		-s exit:0 grep "^foo" ${HOME}/master.passwd
	atf_check -s exit:0 ${PW} userdel foo
	atf_check -s exit:0 \
		${PW} useradd foo -e 20-Mar-2043
	atf_check -o inline:"foo:*:1001:1001::0:2310422400:User &:/home/foo:/bin/sh\n" \
		-s exit:0 grep "^foo" ${HOME}/master.passwd
	atf_check -s exit:0 ${PW} userdel foo
	atf_check -e inline:"pw: Invalid date\n" -s exit:1 \
		${PW} useradd foo -e 20-Foo-2043
	atf_check -e inline:"pw: Invalid date\n" -s exit:1 \
		${PW} useradd foo -e 20-13-2043
	atf_check -s exit:0 ${PW} useradd foo -e "12:00 20-03-2043"
	atf_check -s exit:0 ${PW} userdel foo
	atf_check -e inline:"pw: Invalid date\n" -s exit:1 \
		${PW} useradd foo -e "12 20-03-2043"
	atf_check -s exit:0 ${PW} useradd foo -e "20-03-2043	12:00"
	atf_check -s exit:0 ${PW} userdel foo
}

atf_test_case user_add_invalid_user_entry
user_add_invalid_user_entry_body() {
	touch ${HOME}/master.passwd
	touch ${HOME}/group

	pwd_mkdb -p -d ${HOME} ${HOME}/master.passwd || \
		atf_fail "generate passwd from master.passwd"
	atf_check -s exit:0 ${PW} useradd foo
	echo "foo1:*:1002" >> ${HOME}/master.passwd
	atf_check -s exit:1 -e match:"Invalid user entry" ${PW} useradd foo2
}

atf_test_case user_add_invalid_group_entry
user_add_invalid_group_entry_body() {
	touch ${HOME}/master.passwd
	touch ${HOME}/group

	pwd_mkdb -p -d ${HOME} ${HOME}/master.passwd || \
		atf_fail "generate passwd from master.passwd"
	atf_check -s exit:0 ${PW} useradd foo
	echo 'foo1:*:1002' >> group
	atf_check -s exit:1 -e match:"Invalid group entry" ${PW} useradd foo2
}

atf_test_case user_add_password_from_h
user_add_password_from_h_body() {
	populate_etc_skel

	atf_check -s exit:0 ${PW} useradd test -h 0 <<-EOF
	$(echo test)
	EOF
}

atf_init_test_cases() {
	atf_add_test_case user_add
	atf_add_test_case user_add_noupdate
	atf_add_test_case user_add_comments
	atf_add_test_case user_add_comments_noupdate
	atf_add_test_case user_add_comments_invalid
	atf_add_test_case user_add_comments_invalid_noupdate
	atf_add_test_case user_add_homedir
	atf_add_test_case user_add_account_expiration_epoch
	atf_add_test_case user_add_account_expiration_date_numeric
	atf_add_test_case user_add_account_expiration_date_month
	atf_add_test_case user_add_account_expiration_date_relative
	atf_add_test_case user_add_password_expiration_epoch
	atf_add_test_case user_add_password_expiration_date_numeric
	atf_add_test_case user_add_password_expiration_date_month
	atf_add_test_case user_add_password_expiration_date_relative
	atf_add_test_case user_add_name_too_long
	atf_add_test_case user_add_expiration
	atf_add_test_case user_add_invalid_user_entry
	atf_add_test_case user_add_invalid_group_entry
	atf_add_test_case user_add_password_from_h
}
