# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2017 Alan Somers
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

atf_test_case special
special_head() {
	atf_set "descr" "Test cmp(1)'s handling of non-regular files"
}
special_body() {
	echo 0123456789abcdef > a
	echo 0123456789abcdeg > b
	atf_check -s exit:0 -o empty -e empty cmp a - <a
	atf_check -s exit:0 -o empty -e empty cmp - a <a
	atf_check -s exit:1 -o not-empty -e empty cmp a - <b
	atf_check -s exit:1 -o not-empty -e empty cmp - a <b

	atf_check -s exit:0 -o empty -e empty cmp a a <&-
}

atf_test_case symlink
symlink_head() {
	atf_set "descr" "Test cmp(1)'s handling of symlinks"
}
symlink_body() {
	echo 0123456789abcdef > a
	echo 0123456789abcdeg > b
	ln -s a a.lnk
	ln -s b b.lnk
	ln -s a a2.lnk
	cp a adup
	ln -s adup adup.lnk
	atf_check -s exit:0 cmp a a.lnk
	atf_check -s exit:0 cmp a.lnk a
	atf_check -s not-exit:0 -o ignore cmp a b.lnk
	atf_check -s not-exit:0 -o ignore cmp b.lnk a
	atf_check -s not-exit:0 -o ignore -e ignore cmp -h a a.lnk
	atf_check -s not-exit:0 -o ignore -e ignore cmp -h a.lnk a
	atf_check -s exit:0 cmp -h a.lnk a2.lnk
	atf_check -s not-exit:0 -o ignore -e ignore cmp -h a.lnk adup.lnk
}

atf_test_case pr252542
pr252542_head()
{
	atf_set "descr" "Test cmp(1) -s with file offset skips"
}
pr252542_body()
{
	echo -n '1234567890' > a
	echo -n 'abc567890' > b
	echo -n 'xbc567890' > c
	atf_check -s exit:0 cmp -s a b 4 3
	atf_check -s exit:0 cmp -i 4:3 -s a b
	atf_check -s exit:0 cmp -i 1 -s b c
	atf_check -s exit:1 -o ignore cmp -z a b 4 3
	atf_check -s exit:1 -o ignore cmp -i 4:3 -z a b
	atf_check -s exit:1 -o ignore cmp -i 1 -z a b
}

atf_test_case skipsuff
skipsuff_head()
{
	atf_set "descr" "Test cmp(1) accepting SI suffixes on skips"
}
skipsuff_body()
{

	jot -nb a -s '' 1028 > a
	jot -nb b -s '' 1024 > b
	jot -nb a -s '' 4 >> b

	atf_check -s exit:1 -o ignore cmp -s a b
	atf_check -s exit:0 cmp -s a b 1k 1k
}

atf_test_case limit
limit_head()
{
	atf_set "descr" "Test cmp(1) -n (limit)"
}
limit_body()
{
	echo -n "aaaabbbb" > a
	echo -n "aaaaxxxx" > b

	atf_check -s exit:1 -o ignore cmp -s a b
	atf_check -s exit:0 cmp -sn 4 a b
	atf_check -s exit:0 cmp -sn 3 a b
	atf_check -s exit:1 -o ignore cmp -sn 5 a b

	# Test special, too.  The implementation for link is effectively
	# identical.
	atf_check -s exit:0 -e empty cmp -sn 4 b - <a
	atf_check -s exit:0 -e empty cmp -sn 3 b - <a
	atf_check -s exit:1 -o ignore cmp -sn 5 b - <a
}

atf_test_case bflag
bflag_head()
{
	atf_set "descr" "Test cmp(1) -b (print bytes)"
}
bflag_body()
{
	echo -n "abcd" > a
	echo -n "abdd" > b

	atf_check -s exit:1 -o file:$(atf_get_srcdir)/b_flag.out \
	    cmp -b a b
	atf_check -s exit:1 -o file:$(atf_get_srcdir)/bl_flag.out \
	    cmp -bl a b
}

# Helper for stdout test case
atf_check_stdout()
{
	(
		trap "" PIPE
		sleep 1
		cmp "$@" 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"2\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_test_case stdout
stdout_head()
{
	atf_set descr "Failure to write to stdout"
}
stdout_body()
{
	echo a >a
	echo b >b
	atf_check_stdout a b
	atf_check_stdout - b <a
	atf_check_stdout a - <b
	ln -s a alnk
	ln -s b blnk
	atf_check_stdout -h alnk blnk
}

atf_init_test_cases()
{
	atf_add_test_case special
	atf_add_test_case symlink
	atf_add_test_case pr252542
	atf_add_test_case skipsuff
	atf_add_test_case limit
	atf_add_test_case bflag
	atf_add_test_case stdout
}
