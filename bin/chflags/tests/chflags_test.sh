#
# Copyright 2017 Shivansh Rai
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

usage_output='usage: chflags'

# Skip the calling test if the work filesystem does not support
# setting the uchg file flag (e.g. some ZFS configurations).
require_chflags()
{
	touch .chflags_probe
	if ! chflags uchg .chflags_probe 2>/dev/null; then
		rm -f .chflags_probe
		atf_skip "filesystem does not support the uchg flag"
	fi
	chflags nouchg .chflags_probe
	rm -f .chflags_probe
}

atf_test_case invalid_usage
invalid_usage_head()
{
	atf_set "descr" "Verify that an invalid usage with a supported option produces a valid error message"
}

invalid_usage_body()
{
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -f
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -H
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -h
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -L
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -P
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -R
	atf_check -s not-exit:0 -e match:"$usage_output" chflags -v
}

atf_test_case no_arguments
no_arguments_head()
{
	atf_set "descr" "Verify that chflags(1) fails and generates a valid usage message when no arguments are supplied"
}

no_arguments_body()
{
	atf_check -s not-exit:0 -e match:"$usage_output" chflags
}

atf_test_case relative_path
relative_path_head()
{
	atf_set "descr" "chflags sets flags on a relative path"
}
relative_path_body()
{
	require_chflags
	touch file
	atf_check chflags uchg file
	atf_check -o match:uchg stat -f "%Sf" file
	atf_check chflags nouchg file
}

atf_test_case absolute_path
absolute_path_head()
{
	atf_set "descr" "chflags sets flags on an absolute path"
}
absolute_path_body()
{
	require_chflags
	touch file
	atf_check chflags uchg "$(pwd)/file"
	atf_check -o match:uchg stat -f "%Sf" file
	atf_check chflags nouchg "$(pwd)/file"
}

atf_test_case dotdot_path
dotdot_path_head()
{
	atf_set "descr" "chflags sets flags on a path containing .."
}
dotdot_path_body()
{
	require_chflags
	mkdir dir
	touch file
	cd dir
	atf_check chflags uchg ../file
	atf_check -o match:uchg stat -f "%Sf" ../file
	atf_check chflags nouchg ../file
}

atf_test_case recursive
recursive_head()
{
	atf_set "descr" "chflags -R sets flags on a directory tree"
}
recursive_body()
{
	require_chflags
	mkdir -p dir/sub
	touch dir/file dir/sub/file
	atf_check chflags -R uchg dir
	atf_check -o match:uchg stat -f "%Sf" dir/file
	atf_check -o match:uchg stat -f "%Sf" dir/sub/file
	atf_check chflags -R nouchg dir
}

atf_test_case mixed_paths
mixed_paths_head()
{
	atf_set "descr" "chflags handles relative, absolute and .. paths together"
}
mixed_paths_body()
{
	require_chflags
	mkdir dir
	touch a dir/b c
	cd dir
	atf_check chflags uchg b "$(pwd)/../a" ../c
	atf_check -o match:uchg stat -f "%Sf" b
	atf_check -o match:uchg stat -f "%Sf" ../a
	atf_check -o match:uchg stat -f "%Sf" ../c
	atf_check chflags nouchg b "$(pwd)/../a" ../c
}

atf_test_case outside_symlink_rejected
outside_symlink_rejected_head()
{
	atf_set "descr" "chflags -RL does not follow a symlink pointing " \
	    "outside the traversal by default"
}
outside_symlink_rejected_body()
{
	require_chflags
	mkdir -p foo/bar/baz
	touch target
	ln -s ../../../target foo/bar/baz/link
	atf_check -s not-exit:0 -e ignore chflags -RL uchg foo/bar
	atf_check -o match:"target[[:space:]]*-" stat -f "%N %Sf" target
}

outside_symlink_rejected_cleanup()
{
	chflags -R --dereference-links-unsafely 0 foo target 2>/dev/null || true
}

atf_test_case outside_symlink_unsafe
outside_symlink_unsafe_head()
{
	atf_set "descr" "chflags -RL --dereference-links-unsafely follows " \
	    "a symlink pointing outside the traversal"
}
outside_symlink_unsafe_body()
{
	require_chflags
	mkdir -p foo/bar/baz
	touch target
	ln -s ../../../target foo/bar/baz/link
	atf_check chflags -RL --dereference-links-unsafely uchg foo/bar
	atf_check -o match:uchg stat -f "%Sf" target
	atf_check chflags --dereference-links-unsafely nouchg target
}

outside_symlink_unsafe_cleanup()
{
	chflags -R --dereference-links-unsafely 0 foo target 2>/dev/null || true
}

atf_init_test_cases()
{
	atf_add_test_case invalid_usage
	atf_add_test_case no_arguments
	atf_add_test_case relative_path
	atf_add_test_case absolute_path
	atf_add_test_case dotdot_path
	atf_add_test_case recursive
	atf_add_test_case mixed_paths
	atf_add_test_case outside_symlink_rejected
	atf_add_test_case outside_symlink_unsafe
}
