#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Kyle Evans <kevans@FreeBSD.org>
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

# What grep(1) are we working with?
# - 0 : bsdgrep
# - 1 : gnu grep (ports)
GREP_TYPE_BSD=0
GREP_TYPE_GNU=1

grep_type()
{
	local grep_version=$(grep --version)

	case "$grep_version" in
	*"BSD grep"*)
		return $GREP_TYPE_BSD
		;;
	*"GNU grep"*)
		return $GREP_TYPE_GNU
		;;
	esac
	atf_fail "unknown grep type: $grep_version"
}

atf_test_case grep_r_implied
grep_r_implied_body()
{
	grep_type
	if [ $? -ne $GREP_TYPE_BSD ]; then
		atf_skip "this test only works with bsdgrep(1)"
	fi

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
	atf_check -o save:d_grep_r_implied.out grep -r --exclude="*.out" -e "test" "$(atf_get_srcdir)"
	atf_check -o file:d_grep_r_implied.out rgrep --exclude="*.out" -e "test" "$(atf_get_srcdir)"
}

atf_test_case gnuext
gnuext_body()
{
	grep_type
	_type=$?

	atf_check -o save:grep_alnum.out grep -o '[[:alnum:]]' /COPYRIGHT
	atf_check -o file:grep_alnum.out grep -o '\w' /COPYRIGHT

	atf_check -o save:grep_nalnum.out grep -o '[^[:alnum:]]' /COPYRIGHT
	atf_check -o file:grep_nalnum.out grep -o '\W' /COPYRIGHT

	atf_check -o save:grep_space.out grep -o '[[:space:]]' /COPYRIGHT
	atf_check -o file:grep_space.out grep -o '\s' /COPYRIGHT

	atf_check -o save:grep_nspace.out grep -o '[^[:space:]]' /COPYRIGHT
	atf_check -o file:grep_nspace.out grep -o '\S' /COPYRIGHT

}

atf_test_case zflag
zflag_body()
{

	# The -z flag should pick up 'foo' and 'bar' as on the same line with
	# 'some kind of junk' in between; a bug was present that instead made
	# it process this incorrectly.
	printf "foo\nbar\0" > in

	atf_check grep -qz "foo.*bar" in
}

atf_test_case color_dupe
color_dupe_body()
{

	# This assumes a MAX_MATCHES of exactly 32.  Previously buggy procline()
	# calls would terminate the line premature every MAX_MATCHES matches,
	# meaning we'd see the line be output again for the next MAX_MATCHES
	# number of matches.
	jot -nb 'A' -s '' 33 > in

	atf_check -o save:color.out grep --color=always . in
	atf_check -o match:"^ +1 color.out" wc -l color.out
}

atf_init_test_cases()
{
	atf_add_test_case grep_r_implied
	atf_add_test_case rgrep
	atf_add_test_case gnuext
	atf_add_test_case zflag
	atf_add_test_case color_dupe
}
