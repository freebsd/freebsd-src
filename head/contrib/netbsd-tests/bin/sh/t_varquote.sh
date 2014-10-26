# $NetBSD: t_varquote.sh,v 1.2 2012/03/25 18:50:19 christos Exp $
#
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

# Variable quoting test.

check() {
	if [ "$1" != "$2" ]
	then
		atf_fail "expected [$2], found [$1]" 1>&2
	fi
}

atf_test_case all
all_head() {
	atf_set "descr" "Basic checks for variable quoting"
}
all_body() {
	foo='${a:-foo}'
	check "$foo" '${a:-foo}'

	foo="${a:-foo}"
	check "$foo" "foo"

	foo=${a:-"'{}'"}
	check "$foo" "'{}'"

	foo=${a:-${b:-"'{}'"}}
	check "$foo" "'{}'"

	foo="${a:-"'{}'"}"
	check "$foo" "'{}'"

	foo="${a:-${b:-"${c:-${d:-"x}"}}y}"}}z}"
	#   "                                z*"
	#    ${a:-                          }
	#         ${b:-                    }
	#              "                y*"
	#               ${c:-          }
	#                    ${d:-    }
	#                         "x*"
	check "$foo" "x}y}z}"
}

atf_test_case nested_quotes_multiword
nested_quotes_multiword_head() {
	atf_set "descr" "Tests that having nested quoting in a multi-word" \
	    "string works (PR bin/43597)"
}
nested_quotes_multiword_body() {
	atf_check -s eq:0 -o match:"first-word second-word" -e empty \
	    /bin/sh -c 'echo "${foo:="first-word"} second-word"'
}

atf_init_test_cases() {
	atf_add_test_case all
	atf_add_test_case nested_quotes_multiword
}
