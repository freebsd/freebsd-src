#
# SPDX-License-Identifier: BSD-2-Clause
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

atf_check_same_file()
{
	atf_check_equal "$(stat -f %d,%i "$1")" "$(stat -f %d,%i "$2")"
}

atf_check_symlink_to()
{
	atf_check -o inline:"$1\n" readlink "$2"
}

atf_test_case L_flag
L_flag_head()
{
	atf_set "descr" "Verify that when creating a hard link to a " \
			"symbolic link, '-L' option creates a hard" \
			"link to the target of the symbolic link"
}
L_flag_body()
{
	atf_check touch A
	atf_check ln -s A B
	atf_check ln -L B C
	atf_check_same_file A C
	atf_check_symlink_to A B
}

atf_test_case P_flag
P_flag_head()
{
	atf_set "descr" "Verify that when creating a hard link to a " \
			"symbolic link, '-P' option creates a hard " \
			"link to the symbolic link itself"
}
P_flag_body()
{
	atf_check touch A
	atf_check ln -s A B
	atf_check ln -P B C
	atf_check_same_file B C
}

atf_test_case f_flag
f_flag_head()
{
	atf_set "descr" "Verify that if the target file already exists, " \
			"'-f' option unlinks it so that link may occur"
}
f_flag_body()
{
	atf_check touch A B
	atf_check ln -f A B
	atf_check_same_file A B
}

atf_test_case target_exists_hard
target_exists_hard_head()
{
	atf_set "descr" "Verify whether creating a hard link fails if the " \
			"target file already exists"
}
target_exists_hard_body()
{
	atf_check touch A B
	atf_check -s exit:1 -e inline:'ln: B: File exists\n' \
	    ln A B
}

atf_test_case target_exists_symbolic
target_exists_symbolic_head()
{
	atf_set "descr" "Verify whether creating a symbolic link fails if " \
			"the target file already exists"
}
target_exists_symbolic_body()
{
	atf_check touch A B
	atf_check -s exit:1 -e inline:'ln: B: File exists\n' \
	    ln -s A B
}

atf_test_case shf_flag_dir
shf_flag_dir_head() {
	atf_set "descr" "Verify that if the target directory is a symbolic " \
			"link, '-shf' option prevents following the link"
}
shf_flag_dir_body()
{
	atf_check mkdir -m 0777 A B
	atf_check ln -s A C
	atf_check ln -shf B C
	atf_check test -L C
	atf_check -o inline:'B\n' readlink C
}

atf_test_case snf_flag_dir
snf_flag_dir_head() {
	atf_set "descr" "Verify that if the target directory is a symbolic " \
			"link, '-snf' option prevents following the link"
}
snf_flag_dir_body()
{
	atf_check mkdir -m 0777 A B
	atf_check ln -s A C
	atf_check ln -snf B C
	atf_check_symlink_to B C
}

atf_test_case sF_flag
sF_flag_head()
{
	atf_set "descr" "Verify that if the target file already exists " \
			"and is a directory, then '-sF' option removes " \
			"it so that the link may occur"
}
sF_flag_body()
{
	atf_check mkdir A B
	atf_check ln -sF A B
	atf_check_symlink_to A B
}

atf_test_case sf_flag
sf_flag_head()
{
	atf_set "descr" "Verify that if the target file already exists, " \
			"'-sf' option unlinks it and creates a symbolic link " \
			"to the source file"
}
sf_flag_body()
{
	atf_check touch A B
	atf_check ln -sf A B
	atf_check_symlink_to A B
}

atf_test_case sfF_flag
sfF_flag_head()
{
	atf_set "descr" "Verify that if the target file already exists " \
			"and is a symlink, then '-sfF' option removes " \
			"it so that the link may occur"
}
sfF_flag_body()
{
	atf_check mkdir A B C
	atf_check ln -sF A C
	atf_check_symlink_to A C
	atf_check ln -sfF B C
	atf_check_symlink_to B C
}

atf_test_case s_flag
s_flag_head()
{
	atf_set "descr" "Verify that '-s' option creates a symbolic link"
}
s_flag_body()
{
	atf_check touch A
	atf_check ln -s A B
	atf_check_symlink_to A B
}

atf_test_case s_flag_broken
s_flag_broken_head()
{
	atf_set "descr" "Verify that if the source file does not exists, '-s' " \
			"option creates a broken symbolic link to the source file"
}
s_flag_broken_body()
{
	atf_check ln -s A B
	atf_check_symlink_to A B
}

atf_test_case sw_flag
sw_flag_head()
{
	atf_set "descr" "Verify that '-sw' option produces a warning if the " \
			"source of a symbolic link does not currently exist"
}
sw_flag_body()
{
	atf_check -s exit:0 -e inline:'ln: warning: A: No such file or directory\n' \
	    ln -sw A B
	atf_check_symlink_to A B
}

atf_test_case link_argc
link_argc_head() {
	atf_set "descr" "Verify that link(1) requires exactly two arguments"
}
link_argc_body() {
	atf_check -s exit:1 -e match:"usage: link" \
	    link foo
	atf_check -s exit:1 -e match:"No such file" \
	    link foo bar
	atf_check -s exit:1 -e match:"No such file" \
	    link -- foo bar
	atf_check -s exit:1 -e match:"usage: link" \
	    link foo bar baz
}

atf_test_case link_basic
link_basic_head() {
	atf_set "descr" "Verify that link(1) creates a link"
}
link_basic_body() {
	touch foo
	atf_check link foo bar
	atf_check_same_file foo bar
	rm bar
	ln -s foo bar
	atf_check link bar baz
	atf_check_same_file foo baz
}

atf_test_case link_eexist
link_eexist_head() {
	atf_set "descr" "Verify that link(1) fails if the target exists"
}
link_eexist_body() {
	touch foo bar
	atf_check -s exit:1 -e match:"bar.*exists" \
	    link foo bar
	ln -s non-existent baz
	atf_check -s exit:1 -e match:"baz.*exists" \
	    link foo baz
}

atf_test_case link_eisdir
link_eisdir_head() {
	atf_set "descr" "Verify that link(1) fails if the source is a directory"
}
link_eisdir_body() {
	mkdir foo
	atf_check -s exit:1 -e match:"foo.*directory" \
	    link foo bar
	ln -s foo bar
	atf_check -s exit:1 -e match:"bar.*directory" \
	    link bar baz
}

atf_init_test_cases()
{
	atf_add_test_case L_flag
	atf_add_test_case P_flag
	atf_add_test_case f_flag
	atf_add_test_case target_exists_hard
	atf_add_test_case target_exists_symbolic
	atf_add_test_case shf_flag_dir
	atf_add_test_case snf_flag_dir
	atf_add_test_case sF_flag
	atf_add_test_case sf_flag
	atf_add_test_case sfF_flag
	atf_add_test_case s_flag
	atf_add_test_case s_flag_broken
	atf_add_test_case sw_flag
	atf_add_test_case link_argc
	atf_add_test_case link_basic
	atf_add_test_case link_eexist
	atf_add_test_case link_eisdir
}
