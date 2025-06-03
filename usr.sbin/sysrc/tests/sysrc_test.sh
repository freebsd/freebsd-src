#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 Michal Scigocki <michal.os@hotmail.com>
#

readonly SYSRC_CONF="${PWD}/sysrc_test.conf"

atf_test_case check_variable
check_variable_head()
{
	atf_set "descr" "Check rc variable"
}
check_variable_body()
{
	cat <<- EOF > "${SYSRC_CONF}"
		test_enable="NO"
		test_list="item1 item2"
		test2_list=""
	EOF

	atf_check -o inline:"test_enable: NO\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable
	atf_check -o inline:"test2_list: \n" \
	    sysrc -f "${SYSRC_CONF}" test2_list
	atf_check -o inline:"test_enable: NO\ntest_list: item1 item2\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable test_list
	atf_check -e inline:"sysrc: unknown variable 'noexist'\n" \
	    -s exit:1 sysrc -f "${SYSRC_CONF}" noexist
	atf_check -e inline:"sysrc: unknown variable 'noexist'\n" \
	    -s exit:1 -o inline:"test_enable: NO\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable noexist
}

atf_test_case set_variable
set_variable_head()
{
	atf_set "descr" "Set rc variable"
}
set_variable_body()
{
	# Previously unset variable
	atf_check -o inline:"test_enable:  -> YES\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable=YES
	atf_check -o inline:'test_enable="YES"\n' cat "${SYSRC_CONF}"
	# Change set variable
	atf_check -o inline:"test_enable: YES -> YES\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable=YES
	atf_check -o inline:'test_enable="YES"\n' cat "${SYSRC_CONF}"
	atf_check -o inline:"test_enable: YES -> NO\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable=NO
	atf_check -o inline:'test_enable="NO"\n' cat "${SYSRC_CONF}"
	# Set an empty variable
	atf_check -o inline:"test2_enable:  -> \n" \
	    sysrc -f "${SYSRC_CONF}" test2_enable=""
	atf_check -o inline:'test_enable="NO"\ntest2_enable=""\n' \
	    cat "${SYSRC_CONF}"
}

atf_test_case remove_variable_x_flag
remove_variable_x_flag_head()
{
	atf_set "descr" "Remove rc variable (-x flag)"
}
remove_variable_x_flag_body()
{
	cat <<- EOF > "${SYSRC_CONF}"
		test1_enable="YES"
		test2_enable="NO"
		test3_enable=""
	EOF

	atf_check sysrc -f "${SYSRC_CONF}" -x test1_enable
	atf_check -o inline:'test2_enable="NO"\ntest3_enable=""\n' \
	    cat "${SYSRC_CONF}"
	atf_check sysrc -f "${SYSRC_CONF}" -x test2_enable test3_enable
	atf_check -o empty cat "${SYSRC_CONF}"
	atf_check -e inline:"sysrc: unknown variable 'noexist'\n"\
	    -s exit:1 sysrc -f "${SYSRC_CONF}" -x noexist
}

atf_test_case append_list
append_list_head()
{
	atf_set "descr" "Append rc variable to list"
}
append_list_body()
{
	atf_check -o inline:"test_list:  -> item1\n" \
	    sysrc -f "${SYSRC_CONF}" test_list+=item1
	atf_check -o inline:'test_list="item1"\n' cat "${SYSRC_CONF}"
	atf_check -o inline:"test_list: item1 -> item1\n" \
	    sysrc -f "${SYSRC_CONF}" test_list+=item1
	atf_check -o inline:'test_list="item1"\n' cat "${SYSRC_CONF}"
	atf_check -o inline:"test_list: item1 -> item1 item2\n" \
	    sysrc -f "${SYSRC_CONF}" test_list+=item2
	atf_check -o inline:'test_list="item1 item2"\n' \
	    cat "${SYSRC_CONF}"
}

atf_test_case subtract_list
subtract_list_head()
{
	atf_set "descr" "Subtract rc variable from list"
}
subtract_list_body()
{
	echo 'test_list="item1 item2"' > "${SYSRC_CONF}"

	atf_check -o inline:"test_list: item1 item2 -> item1\n" \
	    sysrc -f "${SYSRC_CONF}" test_list-=item2
	atf_check -o inline:'test_list="item1"\n' cat "${SYSRC_CONF}"
	atf_check -o inline:"test_list: item1 -> \n" \
	    sysrc -f "${SYSRC_CONF}" test_list-=item1
	atf_check -o inline:'test_list=""\n' cat "${SYSRC_CONF}"
	atf_check -o inline:"test_list:  -> \n" \
	    sysrc -f "${SYSRC_CONF}" test_list-=item3
	atf_check -o inline:'test_list=""\n' cat "${SYSRC_CONF}"

	# Subtracting a non-existant rc variable results in an empty variable
	# being created. This is by design, see sysrc(8).
	atf_check -s exit:1 grep test2_list "${SYSRC_CONF}"
	atf_check -o inline:"test2_list:  -> \n" \
	    sysrc -f "${SYSRC_CONF}" test2_list-=item1
	atf_check -o inline:'test_list=""\ntest2_list=""\n' \
	    cat "${SYSRC_CONF}"
}

atf_test_case multiple_operations
multiple_operations_head()
{
	atf_set "descr" "Check performing multiple operations on rc variables"
}
multiple_operations_body()
{
	atf_check -o inline:"test1_enable:  -> YES\ntest1_list:  -> item1\n" \
	    sysrc -f "${SYSRC_CONF}" test1_enable=YES test1_list+=item1
	atf_check -o inline:'test1_enable="YES"\ntest1_list="item1"\n' \
	    cat "${SYSRC_CONF}"

	# The trailing space on line "^test1_list:" is necessary.
	cat <<- EOF > expected
		test1_enable: YES
		test1_enable: YES -> NO
		test2_enable:  -> YES
		test1_list: item1 -> 
		test2_list:  -> item3
	EOF
	atf_check -s exit:1 -e inline:"sysrc: unknown variable 'noexist'\n" \
	    -o file:expected sysrc -f "${SYSRC_CONF}" test1_enable \
	    test1_enable=NO noexist test2_enable=YES test1_list-=item1 \
	    test2_list+=item3

	cat <<- EOF > expected
		test1_enable="NO"
		test1_list=""
		test2_enable="YES"
		test2_list="item3"
	EOF
	atf_check -o file:expected cat "${SYSRC_CONF}"
}

atf_test_case a_flag
a_flag_head()
{
	atf_set "descr" "Verify -a behavior"
}
a_flag_body()
{
	echo 'test_enable="YES"' > "${SYSRC_CONF}"

	atf_check -o inline:"test_enable: YES\n" sysrc -f "${SYSRC_CONF}" -a
}

atf_test_case A_flag cleanup
A_flag_head()
{
	atf_set "descr" "Verify -A behavior"
}
A_flag_body()
{
	RC_DEFAULTS="${PWD}/rc_defaults.conf"
	echo "rc_conf_files=\"${SYSRC_CONF}\"" > "${RC_DEFAULTS}"
	echo 'test_enable="NO"' >> "${RC_DEFAULTS}"
	echo 'test_enable="YES"' > "${SYSRC_CONF}"

	export RC_DEFAULTS

	# sysrc is coupled to the "source_rc_confs" sh(1) script function in
	# /etc/defaults/rc.conf. For this test we use a custom default
	# rc.conf file that is missing the "source_rc_confs" function, which
	# causes sysrc to print an error message. While the coupling exists,
	# we assume the error message is expected behaviour instead of just
	# ignoring the stderr output to make sure we don't ignore other error
	# messages in case of future regressions.
	atf_check -e inline:"/usr/sbin/sysrc: source_rc_confs: not found\n" \
	    -o inline:"rc_conf_files: ${SYSRC_CONF}\ntest_enable: NO\n" \
	    sysrc -A
	# Because "source_rc_confs" does not exist in our custom default
	# rc.conf file, we cannot get sysrc to load the other SYSRC_CONF file
	# to compare results. We can only verify that configuration variables
	# get read from the RC_DEFAULTS file.
}
A_flag_cleanup()
{
	unset RC_DEFAULTS
}

atf_test_case c_flag
c_flag_head()
{
	atf_set "descr" "Verify -c behavior"
}
c_flag_body()
{
	echo 'test_enable="NO"' > "${SYSRC_CONF}"
	echo 'test_list="item1 item2"' >> "${SYSRC_CONF}"

	# Test if rc variable is set
	atf_check sysrc -f "${SYSRC_CONF}" -c test_enable
	atf_check sysrc -f "${SYSRC_CONF}" -c test_list
	atf_check sysrc -f "${SYSRC_CONF}" -c test_enable test_list
	atf_check -e inline:"sysrc: unknown variable 'noexist'\n"\
	    -s exit:1 sysrc -f "${SYSRC_CONF}" -c noexist
	atf_check -e inline:"sysrc: unknown variable 'noexist'\n"\
	    -s exit:1 sysrc -f "${SYSRC_CONF}" -c test_enable noexist

	cat <<- EOF > expected_err
		sysrc: unknown variable 'noexist1'
		sysrc: unknown variable 'noexist2'
	EOF
	atf_check -e file:expected_err -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c noexist1 noexist2

	# Test rc variable assignment
	atf_check sysrc -f "${SYSRC_CONF}" -c test_enable=NO
	atf_check -s exit:1 sysrc -f "${SYSRC_CONF}" -c test_enable=YES
	atf_check -s exit:1 sysrc -f "${SYSRC_CONF}" -c noexist=YES

	# Test appending rc variables
	atf_check sysrc -f "${SYSRC_CONF}" -c test_list+=item1
	atf_check -s exit:1 sysrc -f "${SYSRC_CONF}" -c test_list+=item3
	atf_check -s exit:1 sysrc -f "${SYSRC_CONF}" -c noexist+=item1
	atf_check sysrc -f "${SYSRC_CONF}" -c test_enable=NO test_list+=item1
	atf_check -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c test_enable=YES test_list+=item1
	atf_check -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c test_enable=NO test_list+=item3
	atf_check -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c test_enable=YES test_list+=item3

	# Test subracting rc variables
	atf_check sysrc -f "${SYSRC_CONF}" -c test_list-=item3
	atf_check -s exit:1 sysrc -f "${SYSRC_CONF}" -c test_list-=item1
	atf_check -s exit:1 sysrc -f "${SYSRC_CONF}" -c noexist-=item1
	atf_check sysrc -f "${SYSRC_CONF}" -c test_enable=NO test_list-=item3
	atf_check -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c test_enable=YES test_list-=item3
	atf_check -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c test_enable=NO test_list-=item1
	atf_check -s exit:1 \
	    sysrc -f "${SYSRC_CONF}" -c test_enable=YES test_list-=item1
}

atf_test_case d_flag cleanup
d_flag_head()
{
	atf_set "descr" "Verify -f behavior"
}
d_flag_body()
{
	echo 'test_enable="NO" # Test Description' > "${SYSRC_CONF}"

	export RC_DEFAULTS="${SYSRC_CONF}"

	atf_check -o inline:'test_enable: Test Description\n' \
	    sysrc -d test_enable
}
d_flag_cleanup()
{
	unset RC_DEFAULTS
}

atf_test_case f_flag
f_flag_head()
{
	atf_set "descr" "Verify -f behavior"
}
f_flag_body()
{
	local sysrc2_conf="${PWD}/sysrc2.conf"
	echo 'test_list="item1"' > "${sysrc2_conf}"
	echo 'test_enable="NO"' >> "${sysrc2_conf}"

	atf_check test ! -f "${SYSRC_CONF}"
	atf_check -e inline:"sysrc: unknown variable 'noexist'\n" \
	    -s exit:1 sysrc -f "${SYSRC_CONF}" noexist
	atf_check test ! -f "${SYSRC_CONF}"
	atf_check -o inline:"test_enable:  -> YES\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable=YES
	atf_check test -f "${SYSRC_CONF}"
	atf_check -o inline:"test_enable: YES\n" \
	    sysrc -f "${SYSRC_CONF}" test_enable
	# -f file order impacts final settings
	atf_check -o inline:"test_enable: YES\n" \
	    sysrc -f "${sysrc2_conf}" -f "${SYSRC_CONF}" test_enable
	atf_check -o inline:"test_enable: NO\n" \
	    sysrc -f "${SYSRC_CONF}" -f "${sysrc2_conf}" test_enable
	atf_check -o inline:"test_enable: YES\ntest_list: item1\n" \
	    sysrc -f "${sysrc2_conf}" -f "${SYSRC_CONF}" -a
}

atf_test_case F_flag
F_flag_head()
{
	atf_set "descr" "Verify -F behavior"
}
F_flag_body()
{
	local sysrc2_conf="${PWD}/sysrc2.conf"
	echo 'test_list="item1"' > "${sysrc2_conf}"
	echo 'test_enable="NO"' > "${SYSRC_CONF}"

	atf_check -o inline:"test_enable: ${SYSRC_CONF}\n" \
	    sysrc -f "${SYSRC_CONF}" -F test_enable
	atf_check -o inline:"test_list: ${sysrc2_conf}\n" \
	    sysrc -f "${sysrc2_conf}" -F test_list

	cat <<- EOF > expected
		test_enable: ${SYSRC_CONF}
		test_list: ${sysrc2_conf}
	EOF
	atf_check -o file:expected sysrc -f "${SYSRC_CONF}" \
	    -f ${sysrc2_conf} -F test_enable test_list
}

atf_init_test_cases()
{
	atf_add_test_case check_variable
	atf_add_test_case set_variable
	atf_add_test_case remove_variable_x_flag
	atf_add_test_case append_list
	atf_add_test_case subtract_list
	atf_add_test_case multiple_operations
	atf_add_test_case a_flag
	atf_add_test_case A_flag
	atf_add_test_case c_flag
	atf_add_test_case d_flag
	atf_add_test_case f_flag
	atf_add_test_case F_flag
}
