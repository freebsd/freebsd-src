# $NetBSD: t_grep.sh,v 1.2 2013/05/17 15:39:17 christos Exp $
#
# Copyright (c) 2008, 2009 The NetBSD Foundation, Inc.
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

atf_test_case basic
basic_head()
{
	atf_set "descr" "Checks basic functionality"
}
basic_body()
{ 
	atf_check -o file:"$(atf_get_srcdir)/d_basic.out" -x \
	    'jot 10000 | grep 123'
}

atf_test_case binary
binary_head()
{
	atf_set "descr" "Checks handling of binary files"
}
binary_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_binary.out" grep $(uname) /bin/sh
}

atf_test_case recurse
recurse_head()
{
	atf_set "descr" "Checks recursive searching"
}
recurse_body()
{
	mkdir -p recurse/a/f recurse/d
	echo -e "cod\ndover sole\nhaddock\nhalibut\npilchard" > recurse/d/fish
	echo -e "cod\nhaddock\nplaice" > recurse/a/f/favourite-fish

	atf_check -o file:"$(atf_get_srcdir)/d_recurse.out" grep -r haddock recurse
}

atf_test_case recurse_symlink
recurse_symlink_head()
{
	atf_set "descr" "Checks symbolic link recursion"
}
recurse_symlink_body()
{
	mkdir -p test/c/d
	(cd test/c/d && ln -s ../d .)
	echo "Test string" > test/c/match

	atf_check -o file:"$(atf_get_srcdir)/d_recurse_symlink.out" \
	    -e file:"$(atf_get_srcdir)/d_recurse_symlink.err" \
	    grep -r string test
}

atf_test_case word_regexps
word_regexps_head()
{
	atf_set "descr" "Checks word-regexps"
}
word_regexps_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_word_regexps.out" \
	    grep -w separated $(atf_get_srcdir)/d_input
}

atf_test_case begin_end
begin_end_head()
{
	atf_set "descr" "Checks handling of line beginnings and ends"
}
begin_end_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_begin_end_a.out" \
	    grep ^Front "$(atf_get_srcdir)/d_input"

	atf_check -o file:"$(atf_get_srcdir)/d_begin_end_b.out" \
	    grep ending$ "$(atf_get_srcdir)/d_input"
}

atf_test_case ignore_case
ignore_case_head()
{
	atf_set "descr" "Checks ignore-case option"
}
ignore_case_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_ignore_case.out" \
	    grep -i Upper "$(atf_get_srcdir)/d_input"
}

atf_test_case invert
invert_head()
{
	atf_set "descr" "Checks selecting non-matching lines with -v option"
}
invert_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_invert.out" \
	    grep -v fish "$(atf_get_srcdir)/d_invert.in"
}

atf_test_case whole_line
whole_line_head()
{
	atf_set "descr" "Checks whole-line matching with -x flag"
}
whole_line_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_whole_line.out" \
	    grep -x matchme "$(atf_get_srcdir)/d_input"
}

atf_test_case negative
negative_head()
{
	atf_set "descr" "Checks handling of files with no matches"
}
negative_body()
{
	atf_check -s ne:0 grep "not a hope in hell" "$(atf_get_srcdir)/d_input"
}

atf_test_case context
context_head()
{
	atf_set "descr" "Checks displaying context with -A, -B and -C flags"
}
context_body()
{
	cp $(atf_get_srcdir)/d_context_*.* .

	atf_check -o file:d_context_a.out grep -C2 bamboo d_context_a.in
	atf_check -o file:d_context_b.out grep -A3 tilt d_context_a.in
	atf_check -o file:d_context_c.out grep -B4 Whig d_context_a.in
	atf_check -o file:d_context_d.out grep -C1 pig d_context_a.in d_context_b.in
}

atf_test_case file_exp
file_exp_head()
{
	atf_set "descr" "Checks reading expressions from file"
}
file_exp_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_file_exp.out" -x \
	    'jot 21 -1 1.00 | grep -f '"$(atf_get_srcdir)"'/d_file_exp.in'
}

atf_test_case egrep
egrep_head()
{
	atf_set "descr" "Checks matching special characters with egrep"
}
egrep_body()
{
	atf_check -o file:"$(atf_get_srcdir)/d_egrep.out" \
		egrep '\?|\*$$' "$(atf_get_srcdir)/d_input"
}

atf_test_case zgrep
zgrep_head()
{
	atf_set "descr" "Checks handling of gzipped files with zgrep"
}
zgrep_body()
{
	cp "$(atf_get_srcdir)/d_input" .
	gzip d_input || atf_fail "gzip failed"

	atf_check -o file:"$(atf_get_srcdir)/d_zgrep.out" zgrep -h line d_input.gz
}

atf_test_case nonexistent
nonexistent_head()
{
	atf_set "descr" "Checks that -s flag suppresses error" \
	                "messages about nonexistent files"
}
nonexistent_body()
{
	atf_check -s ne:0 grep -s foobar nonexistent
}

atf_test_case context2
context2_head()
{
	atf_set "descr" "Checks displaying context with -z flag"
}
context2_body()
{
	printf "haddock\000cod\000plaice\000" > test1
	printf "mackeral\000cod\000crab\000" > test2

	atf_check -o file:"$(atf_get_srcdir)/d_context2_a.out" \
	    grep -z -A1 cod test1 test2

	atf_check -o file:"$(atf_get_srcdir)/d_context2_b.out" \
	    grep -z -B1 cod test1 test2

	atf_check -o file:"$(atf_get_srcdir)/d_context2_c.out" \
	    grep -z -C1 cod test1 test2
}

atf_init_test_cases()
{
	atf_add_test_case basic 
	atf_add_test_case binary
	atf_add_test_case recurse
	atf_add_test_case recurse_symlink
	atf_add_test_case word_regexps
	atf_add_test_case begin_end
	atf_add_test_case ignore_case
	atf_add_test_case invert
	atf_add_test_case whole_line
	atf_add_test_case negative
	atf_add_test_case context
	atf_add_test_case file_exp
	atf_add_test_case egrep
	atf_add_test_case zgrep
	atf_add_test_case nonexistent
	atf_add_test_case context2
}
