# $NetBSD: t_fsplit.sh,v 1.1 2012/03/17 16:33:11 jruoho Exp $
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

# The standard
# http://www.opengroup.org/onlinepubs/009695399/utilities/xcu_chap02.html
# explains (section 2.6) that Field splitting should be performed on the
# result of variable expansions.
# In particular this means that in ${x-word}, 'word' must be expanded as if
# the "${x-" and "}" were absent from the input line.
#
# So: sh -c 'set ${x-a b c}; echo $#' should give 3.
#

nl='
'

check()
{
	result="$(eval $1)"
	# Remove newlines
	oifs="$IFS"
	IFS="$nl"
	result="$(echo $result)"
	IFS="$oifs"
	if [ "$2" != "$result" ]
	then
		atf_fail "expected [$2], found [$result]"
	fi
}

atf_test_case for
for_head() {
	atf_set "descr" "Checks field splitting in for loops"
}
for_body() {
	unset x

	# Since I managed to break this, leave the test in
	check 'for f in $x; do echo x${f}y; done' ''
}

atf_test_case default_val
default_val_head() {
	atf_set "descr" "Checks field splitting in variable default values"
}
default_val_body() {
	unset x

	# Check that IFS is applied to text from ${x-...} unless it is inside
	# any set of "..."
	check 'set ${x-a b c}; echo $#' 3
	check 'for i in ${x-a b c};            do echo "z${i}z"; done' 'zaz zbz zcz'
	check 'for i in ${x-"a b" c};          do echo "z${i}z"; done' 'za bz zcz'
	check 'for i in ${x-"a ${x-b c}" d};   do echo "z${i}z"; done' 'za b cz zdz'
	check 'for i in ${x-"a ${x-"b c"}" d}; do echo "z${i}z"; done' 'za b cz zdz'
	check 'for i in ${x-a ${x-"b c"} d};   do echo "z${i}z"; done' 'zaz zb cz zdz'
	check 'for i in ${x-a ${x-b c} d};     do echo "z${i}z"; done' 'zaz zbz zcz zdz'
}

atf_test_case ifs_alpha
ifs_alpha_head() {
	atf_set "descr" "Checks that field splitting works with alphabetic" \
	                "characters"
}
ifs_alpha_body() {
	unset x

	# repeat with an alphabetic in IFS
	check 'IFS=q; set ${x-aqbqc}; echo $#' 3
	check 'IFS=q; for i in ${x-aqbqc};            do echo "z${i}z"; done' 'zaz zbz zcz'
	check 'IFS=q; for i in ${x-"aqb"qc};          do echo "z${i}z"; done' 'zaqbz zcz'
	check 'IFS=q; for i in ${x-"aq${x-bqc}"qd};   do echo "z${i}z"; done' 'zaqbqcz zdz'
	check 'IFS=q; for i in ${x-"aq${x-"bqc"}"qd}; do echo "z${i}z"; done' 'zaqbqcz zdz'
	check 'IFS=q; for i in ${x-aq${x-"bqc"}qd};  do echo "z${i}z"; done' 'zaz zbqcz zdz'
}

atf_test_case quote
quote_head() {
	atf_set "descr" "Checks that field splitting works with multi-word" \
	                "fields"
}
quote_body() {
	unset x

	# Some quote propagation checks
	check 'set "${x-a b c}";   echo $#' 1
	check 'set "${x-"a b" c}"; echo $1' 'a b c'
	check 'for i in "${x-a b c}"; do echo "z${i}z"; done' 'za b cz'
}

atf_test_case dollar_at
dollar_at_head() {
	atf_set "descr" "Checks that field splitting works when expanding" \
	                "\$@"
}
dollar_at_body() {
	unset x

	# Check we get "$@" right
	check 'set "";        for i;           do echo "z${i}z"; done' 'zz'
	check 'set "";        for i in "$@";   do echo "z${i}z"; done' 'zz'
	check 'set "" "";     for i;           do echo "z${i}z"; done' 'zz zz'
	check 'set "" "";     for i in "$@";   do echo "z${i}z"; done' 'zz zz'
	check 'set "" "";     for i in $@;     do echo "z${i}z"; done' ''
	check 'set "a b" c;   for i;           do echo "z${i}z"; done' 'za bz zcz'
	check 'set "a b" c;   for i in "$@";   do echo "z${i}z"; done' 'za bz zcz'
	check 'set "a b" c;   for i in $@;     do echo "z${i}z"; done' 'zaz zbz zcz'
	check 'set " a b " c; for i in "$@";   do echo "z${i}z"; done' 'z a b z zcz'
	check 'set --;        for i in x"$@"x; do echo "z${i}z"; done' 'zxxz'
	check 'set a;         for i in x"$@"x; do echo "z${i}z"; done' 'zxaxz'
	check 'set a b;       for i in x"$@"x; do echo "z${i}z"; done' 'zxaz zbxz'
}

atf_test_case ifs
ifs_head() {
	atf_set "descr" "Checks that IFS correctly configures field" \
	                "splitting behavior"
}
ifs_body() {
	unset x

	# Some IFS tests
	check 't="-- ";    IFS=" ";  set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '0'
	check 't=" x";     IFS=" x"; set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '1'
	check 't=" x ";    IFS=" x"; set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '1'
	check 't=axb;      IFS="x";  set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '2 a:b'
	check 't="a x b";  IFS="x";  set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '2 a : b'
	check 't="a xx b"; IFS="x";  set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '3 a :: b'
	check 't="a xx b"; IFS="x "; set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '3 a::b'
	# A recent 'clarification' means that a single trailing IFS non-whitespace
	# doesn't generate an empty parameter
	check 't="xax";  IFS="x";     set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '2 :a'
	check 't="xax "; IFS="x ";   set $t; IFS=":"; r="$*"; IFS=; echo $# $r' '2 :a'
	# Verify that IFS isn't being applied where it shouldn't be.
	check 'IFS="x";             set axb; IFS=":"; r="$*"; IFS=; echo $# $r' '1 axb'
}

atf_test_case var_length
var_length_head() {
	atf_set "descr" "Checks that field splitting works when expanding" \
	                "a variable's length"
}
var_length_body() {
	unset x

	# Check that we apply IFS to ${#var}
	long=12345678123456781234567812345678
	long=$long$long$long$long
	check 'echo ${#long}; IFS=2; echo ${#long}; set 1 ${#long};echo $#' '128 1 8 3'
	check 'IFS=2; set ${x-${#long}};   IFS=" "; echo $* $#' '1 8 2'
	check 'IFS=2; set ${x-"${#long}"}; IFS=" "; echo $* $#' '128 1'
}

atf_init_test_cases() {
	atf_add_test_case for
	atf_add_test_case default_val
	atf_add_test_case ifs_alpha
	atf_add_test_case quote
	atf_add_test_case dollar_at
	atf_add_test_case ifs
	atf_add_test_case var_length
}
