# $FreeBSD$

# Import helper functions
. $(atf_get_srcdir)/helper_functions.shin

# Test usernext after adding a random number of new users.
atf_test_case usernext
usernext_body() {
	populate_etc_skel

	var0=1
	LIMIT=`jot -r 1 2 10`
	while [ "$var0" -lt "$LIMIT" ]
	do
		atf_check -s exit:0 ${PW} useradd test$var0
		var0=`expr $var0 + 1`
	done
	atf_check -s exit:0 -o match:"100${LIMIT}:100${LIMIT}" \
		${PW} usernext
}

# Test usernext when multiple users are added to the same group so 
# that group id doesn't increment at the same pace as new users.
atf_test_case usernext_assigned_group
usernext_assigned_group_body() {
	populate_etc_skel

	var0=1
	LIMIT=`jot -r 1 2 10`
	while [ "$var0" -lt "$LIMIT" ]
	do
		atf_check -s exit:0 ${PW} useradd -n test$var0 -g 0
		var0=`expr $var0 + 1`
	done
	atf_check -s exit:0 -o match:"100${LIMIT}:1001" \
		${PW} usernext
}

atf_init_test_cases() {
	atf_add_test_case usernext
	atf_add_test_case usernext_assigned_group
}
