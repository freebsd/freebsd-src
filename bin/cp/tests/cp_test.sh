#
# SPDX-License-Identifier: BSD-2-Clause-FreeBSD
#
# Copyright (c) 2020 Kyle Evans <kevans@FreeBSD.org>
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
# $FreeBSD$

check_size()
{
	file=$1
	sz=$2

	atf_check -o inline:"$sz\n" stat -f '%z' $file
}

atf_test_case basic
basic_body()
{
	echo "foo" > bar

	atf_check cp bar baz
	check_size baz 4
}

atf_test_case basic_symlink
basic_symlink_body()
{
	echo "foo" > bar
	ln -s bar baz

	atf_check cp baz foo
	atf_check test '!' -L foo

	atf_check -e inline:"cp: baz and baz are identical (not copied).\n" \
	    -s exit:1 cp baz baz
	atf_check -e inline:"cp: bar and baz are identical (not copied).\n" \
	    -s exit:1 cp baz bar
}

atf_test_case chrdev
chrdev_body()
{
	echo "foo" > bar

	check_size bar 4
	atf_check cp /dev/null trunc
	check_size trunc 0
	atf_check cp bar trunc
	check_size trunc 4
	atf_check cp /dev/null trunc
	check_size trunc 0
}

recursive_link_setup()
{
	extra_cpflag=$1

	mkdir -p foo/bar
	ln -s bar foo/baz

	mkdir foo-mirror
	eval "cp -R $extra_cpflag foo foo-mirror"
}

atf_test_case recursive_link_dflt
recursive_link_dflt_body()
{
	recursive_link_setup

	# -P is the default, so this should work and preserve the link.
	atf_check cp -R foo foo-mirror
	atf_check test -L foo-mirror/foo/baz
}

atf_test_case recursive_link_Hflag
recursive_link_Hflag_body()
{
	recursive_link_setup

	# -H will not follow either, so this should also work and preserve the
	# link.
	atf_check cp -RH foo foo-mirror
	atf_check test -L foo-mirror/foo/baz
}

atf_test_case recursive_link_Lflag
recursive_link_Lflag_body()
{
	recursive_link_setup -L

	# -L will work, but foo/baz ends up expanded to a directory.
	atf_check test -d foo-mirror/foo/baz -a \
	    '(' ! -L foo-mirror/foo/baz ')'
	atf_check cp -RL foo foo-mirror
	atf_check test -d foo-mirror/foo/baz -a \
	    '(' ! -L foo-mirror/foo/baz ')'
}

atf_test_case standalone_Pflag
standalone_Pflag_body()
{
	echo "foo" > bar
	ln -s bar foo

	atf_check cp -P foo baz
	atf_check -o inline:'Symbolic Link\n' stat -f %SHT baz
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case basic_symlink
	atf_add_test_case chrdev
	atf_add_test_case recursive_link_dflt
	atf_add_test_case recursive_link_Hflag
	atf_add_test_case recursive_link_Lflag
	atf_add_test_case standalone_Pflag
}
