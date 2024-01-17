#-
# Copyright (c) 2023 Lin Lee
#
# SPDX-License-Identifier: BSD-2-Clause
#

#
# These tests need to run in a multibyte locale with non-localized
# error messages.
#
export LC_CTYPE=C.UTF-8
export LC_MESSAGES=C

# root_dir=$(atf_get_srcdir)

test_jail_conf='test-hostname-jail {
    host.hostname = "test-hostname.example.org";
    path = "/";
    persist;
}'

jail_name="test-hostname-jail"

init()
{
    echo "${test_jail_conf}" > "./jail.conf"
    jail -f "./jail.conf" -c $jail_name
}

recycle()
{
    jail -f "./jail.conf" -r $jail_name
    rm "./jail.conf"
}

atf_test_case basic cleanup
basic_head()
{
    atf_set require.user root
    atf_set "descr" "basic test for getting hostname"
}
basic_body()
{
    init
    jexec test-hostname-jail "hostname"
    
    result=$(jexec test-hostname-jail "hostname")
    atf_check_equal "test-hostname.example.org" "${result}"
    
    result=$(jexec test-hostname-jail "hostname" -s)
    atf_check_equal "test-hostname" "${result}"
    
    result=$(jexec test-hostname-jail "hostname" -d)
    atf_check_equal "example.org" "${result}"
    
    jexec test-hostname-jail "hostname" "test-bsd2"
    result=$(jexec test-hostname-jail "hostname")
    atf_check_equal "test-bsd2" "${result}"
}
basic_cleanup()
{
    recycle
}

atf_init_test_cases()
{
    atf_add_test_case basic
}
