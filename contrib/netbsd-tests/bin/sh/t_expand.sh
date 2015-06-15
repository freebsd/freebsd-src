# $NetBSD: t_expand.sh,v 1.2 2013/10/06 21:05:50 ast Exp $
#
# Copyright (c) 2007, 2009 The NetBSD Foundation, Inc.
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

#
# This file tests the functions in expand.c.
#

delim_argv() {
	str=
	while [ $# -gt 0 ]; do
		if [ -z "${str}" ]; then
			str=">$1<"
		else
			str="${str} >$1<"
		fi
		shift
	done
	echo ${str}
}

atf_test_case dollar_at
dollar_at_head() {
	atf_set "descr" "Somewhere between 2.0.2 and 3.0 the expansion" \
	                "of the \$@ variable had been broken.  Check for" \
			"this behavior."
}
dollar_at_body() {
	# This one should work everywhere.
	got=`echo "" "" | sed 's,$,EOL,'`
	atf_check_equal ' EOL' '$got'

	# This code triggered the bug.
	set -- "" ""
	got=`echo "$@" | sed 's,$,EOL,'`
	atf_check_equal ' EOL' '$got'

	set -- -
	shift
	n_arg() { echo $#; }
	n_args=`n_arg "$@"`
	atf_check_equal '0' '$n_args'
}

atf_test_case dollar_at_with_text
dollar_at_with_text_head() {
	atf_set "descr" "Test \$@ expansion when it is surrounded by text" \
	                "within the quotes.  PR bin/33956."
}
dollar_at_with_text_body() {
	set --
	atf_check_equal '' "$(delim_argv "$@")"
	atf_check_equal '>foobar<' "$(delim_argv "foo$@bar")"
	atf_check_equal '>foo  bar<' "$(delim_argv "foo $@ bar")"

	set -- a b c
	atf_check_equal '>a< >b< >c<' "$(delim_argv "$@")"
	atf_check_equal '>fooa< >b< >cbar<' "$(delim_argv "foo$@bar")"
	atf_check_equal '>foo a< >b< >c bar<' "$(delim_argv "foo $@ bar")"
}

atf_test_case strip
strip_head() {
	atf_set "descr" "Checks that the %% operator works and strips" \
	                "the contents of a variable from the given point" \
			"to the end"
}
strip_body() {
	line='#define bindir "/usr/bin" /* comment */'
	stripped='#define bindir "/usr/bin" '
	atf_expect_fail "PR bin/43469"
	atf_check_equal '$stripped' '${line%%/\**}'
}

atf_test_case varpattern_backslashes
varpattern_backslashes_head() {
	atf_set "descr" "Tests that protecting wildcards with backslashes" \
	                "works in variable patterns."
}
varpattern_backslashes_body() {
	line='/foo/bar/*/baz'
	stripped='/foo/bar/'
	atf_check_equal $stripped ${line%%\**}
}

atf_test_case arithmetic
arithmetic_head() {
	atf_set "descr" "POSIX requires shell arithmetic to use signed" \
	                "long or a wider type.  We use intmax_t, so at" \
			"least 64 bits should be available.  Make sure" \
			"this is true."
}
arithmetic_body() {
	atf_check_equal '3' '$((1 + 2))'
	atf_check_equal '2147483647' '$((0x7fffffff))'
	atf_check_equal '9223372036854775807' '$(((1 << 63) - 1))'
}

atf_test_case iteration_on_null_parameter
iteration_on_null_parameter_head() {
	atf_set "descr" "Check iteration of \$@ in for loop when set to null;" \
	                "the error \"sh: @: parameter not set\" is incorrect." \
	                "PR bin/48202."
}
iteration_on_null_parameter_body() {
	s1=`/bin/sh -uc 'N=; set -- ${N};   for X; do echo "[$X]"; done' 2>&1`
	s2=`/bin/sh -uc 'N=; set -- ${N:-}; for X; do echo "[$X]"; done' 2>&1`
	atf_check_equal ''   '$s1'
	atf_check_equal '[]' '$s2'
}

atf_init_test_cases() {
	atf_add_test_case dollar_at
	atf_add_test_case dollar_at_with_text
	atf_add_test_case strip
	atf_add_test_case varpattern_backslashes
	atf_add_test_case arithmetic
	atf_add_test_case iteration_on_null_parameter
}
