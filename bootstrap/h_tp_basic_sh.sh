# Copyright (c) 2007 The NetBSD Foundation, Inc.
# All rights reserved.
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND
# CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
# INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

atf_test_case pass
pass_head()
{
    atf_set "descr" "An empty test case that always passes"
}
pass_body()
{
    atf_pass
}

atf_test_case fail
fail_head()
{
    atf_set "descr" "An empty test case that always fails"
}
fail_body()
{
    atf_fail "On purpose"
}

atf_test_case skip
skip_head()
{
    atf_set "descr" "An empty test case that is always skipped"
}
skip_body()
{
    atf_skip "By design"
}

atf_test_case default
default_head()
{
    atf_set "descr" "A test case that passes without explicitly" \
                    "stating it"
}
default_body()
{
    :
}

atf_init_test_cases()
{
    atf_add_test_case pass
    atf_add_test_case fail
    atf_add_test_case skip
    atf_add_test_case default
}

# vim: syntax=sh:expandtab:shiftwidth=4:softtabstop=4
