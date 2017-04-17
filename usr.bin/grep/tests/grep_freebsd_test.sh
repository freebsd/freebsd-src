#
# Copyright (c) 2017 Kyle Evans <kevans91@ksu.edu>
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
# THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

atf_test_case grep_r_implied
grep_r_implied_body()
{
	(cd "$(atf_get_srcdir)" && grep -r -e "test" < /dev/null) ||
	    atf_skip "Implied working directory is not supported with your version of grep(1)"
	(cd "$(atf_get_srcdir)" && grep -r --exclude="*.out" -e "test" .) > d_grep_r_implied.out

	atf_check -s exit:0 -x \
	    "(cd $(atf_get_srcdir) && grep -r --exclude=\"*.out\" -e \"test\") | diff d_grep_r_implied.out -"
}

atf_test_case rgrep
rgrep_head()
{
	atf_set "require.progs" "rgrep"
}
rgrep_body()
{
	grep -r --exclude="*.out" -e "test" "$(atf_get_srcdir)" > d_grep_r_implied.out

	atf_check -o file:d_grep_r_implied.out rgrep --exclude="*.out" -e "test" "$(atf_get_srcdir)"
}

atf_init_test_cases()
{
	atf_add_test_case grep_r_implied
	atf_add_test_case rgrep
}
