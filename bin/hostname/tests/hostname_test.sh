#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2024 Lin Lee <leelin2602@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#
# These tests need to run in a multibyte locale with non-localized
# error messages.
#
export LC_CTYPE=C.UTF-8
export LC_MESSAGES=C

test_jail_name="test-hostname-jail"

test_jail_conf='%%test_jail_name%% {
    host.hostname = "test-hostname.example.org";
    path = "/";
    persist;
}'

init()
{
    echo "${test_jail_conf}" | \
	    sed -e "s/%%test_jail_name%%/${test_jail_name}/" > "./jail.conf"
    jail -f "./jail.conf" -c ${test_jail_name}
}

recycle()
{
    jail -f "./jail.conf" -r ${test_jail_name}
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

    result=$(jexec ${test_jail_name} "hostname")
    atf_check_equal "test-hostname.example.org" "${result}"

    result=$(jexec ${test_jail_name} "hostname" -s)
    atf_check_equal "test-hostname" "${result}"

    result=$(jexec ${test_jail_name} "hostname" -d)
    atf_check_equal "example.org" "${result}"

    jexec ${test_jail_name} "hostname" "test-bsd2"
    result=$(jexec ${test_jail_name} "hostname")
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
