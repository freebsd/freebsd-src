#
# SPDX-License-Identifier: BSD-2-Clause
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

	atf_check cmp foo bar
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

atf_test_case hardlink
hardlink_body()
{
	echo "foo" >foo
	atf_check cp -l foo bar
	atf_check -o inline:"foo\n" cat bar
	atf_check_equal "$(stat -f%d,%i foo)" "$(stat -f%d,%i bar)"
}

atf_test_case hardlink_exists
hardlink_exists_body()
{
	echo "foo" >foo
	echo "bar" >bar
	atf_check -s not-exit:0 -e match:exists cp -l foo bar
	atf_check -o inline:"bar\n" cat bar
	atf_check_not_equal "$(stat -f%d,%i foo)" "$(stat -f%d,%i bar)"
}

atf_test_case hardlink_exists_force
hardlink_exists_force_body()
{
	echo "foo" >foo
	echo "bar" >bar
	atf_check cp -fl foo bar
	atf_check -o inline:"foo\n" cat bar
	atf_check_equal "$(stat -f%d,%i foo)" "$(stat -f%d,%i bar)"
}

atf_test_case matching_srctgt
matching_srctgt_body()
{

	# PR235438: `cp -R foo foo` would previously infinitely recurse and
	# eventually error out.
	mkdir foo
	echo "qux" > foo/bar
	cp foo/bar foo/zoo

	atf_check cp -R foo foo
	atf_check -o inline:"qux\n" cat foo/foo/bar
	atf_check -o inline:"qux\n" cat foo/foo/zoo
	atf_check -e not-empty -s not-exit:0 stat foo/foo/foo
}

atf_test_case matching_srctgt_contained
matching_srctgt_contained_body()
{

	# Let's do the same thing, except we'll try to recursively copy foo into
	# one of its subdirectories.
	mkdir foo
	ln -s foo coo
	echo "qux" > foo/bar
	mkdir foo/moo
	touch foo/moo/roo
	cp foo/bar foo/zoo

	atf_check cp -R foo foo/moo
	atf_check cp -RH coo foo/moo
	atf_check -o inline:"qux\n" cat foo/moo/foo/bar
	atf_check -o inline:"qux\n" cat foo/moo/coo/bar
	atf_check -o inline:"qux\n" cat foo/moo/foo/zoo
	atf_check -o inline:"qux\n" cat foo/moo/coo/zoo

	# We should have copied the contents of foo/moo before foo, coo started
	# getting copied in.
	atf_check -o not-empty stat foo/moo/foo/moo/roo
	atf_check -o not-empty stat foo/moo/coo/moo/roo
	atf_check -e not-empty -s not-exit:0 stat foo/moo/foo/moo/foo
	atf_check -e not-empty -s not-exit:0 stat foo/moo/coo/moo/coo
}

atf_test_case matching_srctgt_link
matching_srctgt_link_body()
{

	mkdir foo
	echo "qux" > foo/bar
	cp foo/bar foo/zoo

	atf_check ln -s foo roo
	atf_check cp -RH roo foo
	atf_check -o inline:"qux\n" cat foo/roo/bar
	atf_check -o inline:"qux\n" cat foo/roo/zoo
}

atf_test_case matching_srctgt_nonexistent
matching_srctgt_nonexistent_body()
{

	# We'll copy foo to a nonexistent subdirectory; ideally, we would
	# skip just the directory and end up with a layout like;
	#
	# foo/
	#     bar
	#     dne/
	#         bar
	#         zoo
	#     zoo
	#
	mkdir foo
	echo "qux" > foo/bar
	cp foo/bar foo/zoo

	atf_check cp -R foo foo/dne
	atf_check -o inline:"qux\n" cat foo/dne/bar
	atf_check -o inline:"qux\n" cat foo/dne/zoo
	atf_check -e not-empty -s not-exit:0 stat foo/dne/foo
}

atf_test_case pflag_acls
pflag_acls_body()
{
	mkdir dir
	echo "hello" >dir/file
	if ! setfacl -m g:staff:D::allow dir ||
	   ! setfacl -m g:staff:d::allow dir/file ; then
		atf_skip "file system does not support ACLs"
	fi
	atf_check -o match:"group:staff:-+D-+" getfacl dir
	atf_check -o match:"group:staff:-+d-+" getfacl dir/file
	# file-to-file copy without -p
	atf_check cp dir/file dst1
	atf_check -o not-match:"group:staff:-+d-+" getfacl dst1
	# file-to-file copy with -p
	atf_check cp -p dir/file dst2
	atf_check -o match:"group:staff:-+d-+" getfacl dst2
	# recursive copy without -p
	atf_check cp -r dir dst3
	atf_check -o not-match:"group:staff:-+D-+" getfacl dst3
	atf_check -o not-match:"group:staff:-+d-+" getfacl dst3/file
	# recursive copy with -p
	atf_check cp -rp dir dst4
	atf_check -o match:"group:staff:-+D-+" getfacl dst4
	atf_check -o match:"group:staff:-+d-+" getfacl dst4/file
}

atf_test_case pflag_flags
pflag_flags_body()
{
	mkdir dir
	echo "hello" >dir/file
	if ! chflags nodump dir ||
	   ! chflags nodump dir/file ; then
		atf_skip "file system does not support flags"
	fi
	atf_check -o match:"nodump" stat -f%Sf dir
	atf_check -o match:"nodump" stat -f%Sf dir/file
	# file-to-file copy without -p
	atf_check cp dir/file dst1
	atf_check -o not-match:"nodump" stat -f%Sf dst1
	# file-to-file copy with -p
	atf_check cp -p dir/file dst2
	atf_check -o match:"nodump" stat -f%Sf dst2
	# recursive copy without -p
	atf_check cp -r dir dst3
	atf_check -o not-match:"nodump" stat -f%Sf dst3
	atf_check -o not-match:"nodump" stat -f%Sf dst3/file
	# recursive copy with -p
	atf_check cp -rp dir dst4
	atf_check -o match:"nodump" stat -f%Sf dst4
	atf_check -o match:"nodump" stat -f%Sf dst4/file
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

atf_test_case samefile
samefile_body()
{
	echo "foo" >foo
	ln foo bar
	ln -s bar baz
	atf_check -e match:"baz and baz are identical" \
	    -s exit:1 cp baz baz
	atf_check -e match:"bar and baz are identical" \
	    -s exit:1 cp baz bar
	atf_check -e match:"foo and baz are identical" \
	    -s exit:1 cp baz foo
	atf_check -e match:"bar and foo are identical" \
	    -s exit:1 cp foo bar
}

file_is_sparse()
{
	atf_check ${0%/*}/sparse "$1"
}

files_are_equal()
{
	atf_check_not_equal "$(stat -f%d,%i "$1")" "$(stat -f%d,%i "$2")"
	atf_check cmp "$1" "$2"
}

atf_test_case sparse_leading_hole
sparse_leading_hole_body()
{
	# A 16-megabyte hole followed by one megabyte of data
	truncate -s 16M foo
	seq -f%015g 65536 >>foo
	file_is_sparse foo

	atf_check cp foo bar
	files_are_equal foo bar
	file_is_sparse bar
}

atf_test_case sparse_multiple_holes
sparse_multiple_holes_body()
{
	# Three one-megabyte blocks of data preceded, separated, and
	# followed by 16-megabyte holes
	truncate -s 16M foo
	seq -f%015g 65536 >>foo
	truncate -s 33M foo
	seq -f%015g 65536 >>foo
	truncate -s 50M foo
	seq -f%015g 65536 >>foo
	truncate -s 67M foo
	file_is_sparse foo

	atf_check cp foo bar
	files_are_equal foo bar
	file_is_sparse bar
}

atf_test_case sparse_only_hole
sparse_only_hole_body()
{
	# A 16-megabyte hole
	truncate -s 16M foo
	file_is_sparse foo

	atf_check cp foo bar
	files_are_equal foo bar
	file_is_sparse bar
}

atf_test_case sparse_to_dev
sparse_to_dev_body()
{
	# Three one-megabyte blocks of data preceded, separated, and
	# followed by 16-megabyte holes
	truncate -s 16M foo
	seq -f%015g 65536 >>foo
	truncate -s 33M foo
	seq -f%015g 65536 >>foo
	truncate -s 50M foo
	seq -f%015g 65536 >>foo
	truncate -s 67M foo
	file_is_sparse foo

	atf_check -o file:foo cp foo /dev/stdout
}

atf_test_case sparse_trailing_hole
sparse_trailing_hole_body()
{
	# One megabyte of data followed by a 16-megabyte hole
	seq -f%015g 65536 >foo
	truncate -s 17M foo
	file_is_sparse foo

	atf_check cp foo bar
	files_are_equal foo bar
	file_is_sparse bar
}

atf_test_case standalone_Pflag
standalone_Pflag_body()
{
	echo "foo" > bar
	ln -s bar foo

	atf_check cp -P foo baz
	atf_check -o inline:'Symbolic Link\n' stat -f %SHT baz
}

atf_test_case symlink
symlink_body()
{
	echo "foo" >foo
	atf_check cp -s foo bar
	atf_check -o inline:"foo\n" cat bar
	atf_check -o inline:"foo\n" readlink bar
}

atf_test_case symlink_exists
symlink_exists_body()
{
	echo "foo" >foo
	echo "bar" >bar
	atf_check -s not-exit:0 -e match:exists cp -s foo bar
	atf_check -o inline:"bar\n" cat bar
}

atf_test_case symlink_exists_force
symlink_exists_force_body()
{
	echo "foo" >foo
	echo "bar" >bar
	atf_check cp -fs foo bar
	atf_check -o inline:"foo\n" cat bar
	atf_check -o inline:"foo\n" readlink bar
}

atf_test_case directory_to_symlink
directory_to_symlink_body()
{
	mkdir -p foo
	ln -s .. foo/bar
	mkdir bar
	touch bar/baz
	atf_check -s not-exit:0 -e match:"Not a directory" \
	    cp -R bar foo
	atf_check -s not-exit:0 -e match:"Not a directory" \
	    cp -r bar foo
}

atf_test_case overwrite_directory
overwrite_directory_body()
{
	mkdir -p foo/bar/baz
	touch bar
	atf_check -s not-exit:0 -e match:"Is a directory" \
	    cp bar foo
	rm bar
	mkdir bar
	touch bar/baz
	atf_check -s not-exit:0 -e match:"Is a directory" \
	    cp -R bar foo
	atf_check -s not-exit:0 -e match:"Is a directory" \
	    cp -r bar foo
}

atf_test_case to_dir_dne
to_dir_dne_body()
{
	mkdir dir
	echo "foo" >dir/foo
	atf_check cp -r dir dne
	atf_check test -d dne
	atf_check test -f dne/foo
	atf_check cmp dir/foo dne/foo
}

atf_test_case to_nondir
to_nondir_body()
{
	echo "foo" >foo
	echo "bar" >bar
	echo "baz" >baz
	# This is described as “case 1” in source code comments
	atf_check cp foo bar
	atf_check cmp -s foo bar
	# This is “case 2”, the target must be a directory
	atf_check -s not-exit:0 -e match:"Not a directory" \
	    cp foo bar baz
}

atf_test_case to_deadlink
to_deadlink_body()
{
	echo "foo" >foo
	ln -s bar baz
	atf_check cp foo baz
	atf_check cmp -s foo bar
}

atf_test_case to_deadlink_append
to_deadlink_append_body()
{
	echo "foo" >foo
	mkdir bar
	ln -s baz bar/foo
	atf_check cp foo bar
	atf_check cmp -s foo bar/baz
	rm -f bar/foo bar/baz
	ln -s baz bar/foo
	atf_check cp foo bar/
	atf_check cmp -s foo bar/baz
	rm -f bar/foo bar/baz
	ln -s $PWD/baz bar/foo
	atf_check cp foo bar/
	atf_check cmp -s foo baz
}

atf_test_case to_dirlink
to_dirlink_body()
{
	mkdir src dir
	echo "foo" >src/file
	ln -s dir dst
	atf_check cp -r src dst
	atf_check cmp -s src/file dir/src/file
	rm -r dir/*
	atf_check cp -r src dst/
	atf_check cmp -s src/file dir/src/file
	rm -r dir/*
	# If the source is a directory and ends in a slash, our cp has
	# traditionally copied the contents of the source rather than
	# the source itself.  It is unclear whether this is intended
	# or simply a consequence of how FTS handles the situation.
	# Notably, GNU cp does not behave in this manner.
	atf_check cp -r src/ dst
	atf_check cmp -s src/file dir/file
	rm -r dir/*
	atf_check cp -r src/ dst/
	atf_check cmp -s src/file dir/file
	rm -r dir/*
}

atf_test_case to_deaddirlink
to_deaddirlink_body()
{
	mkdir src
	echo "foo" >src/file
	ln -s dir dst
	# It is unclear which error we should expect in these cases.
	# Our current implementation always reports ENOTDIR, but one
	# might be equally justified in expecting EEXIST or ENOENT.
	# GNU cp reports EEXIST when the destination is given with a
	# trailing slash and “cannot overwrite non-directory with
	# directory” otherwise.
	atf_check -s not-exit:0 -e ignore \
	    cp -r src dst
	atf_check -s not-exit:0 -e ignore \
	    cp -r src dst/
	atf_check -s not-exit:0 -e ignore \
	    cp -r src/ dst
	atf_check -s not-exit:0 -e ignore \
	    cp -r src/ dst/
	atf_check -s not-exit:0 -e ignore \
	    cp -R src dst
	atf_check -s not-exit:0 -e ignore \
	    cp -R src dst/
	atf_check -s not-exit:0 -e ignore \
	    cp -R src/ dst
	atf_check -s not-exit:0 -e ignore \
	    cp -R src/ dst/
}

atf_test_case to_link_outside
to_link_outside_body()
{
	mkdir dir dst dst/dir
	echo "foo" >dir/file
	ln -s ../../file dst/dir/file
	atf_check \
	    -s exit:1 \
	    -e match:"dst/dir/file: Permission denied" \
	    cp -r dir dst
}

atf_test_case dstmode
dstmode_body()
{
	mkdir -m 0755 dir
	echo "foo" >dir/file
	umask 0177
	atf_check cp -R dir dst
	umask 022
	atf_check -o inline:"40600\n" stat -f%p dst
	atf_check chmod 0750 dst
	atf_check cmp dir/file dst/file
}

atf_test_case to_root cleanup
to_root_head()
{
	atf_set "require.user" "unprivileged"
}
to_root_body()
{
	dst="test.$(atf_get ident).$$"
	echo "$dst" >dst
	echo "foo" >"$dst"
	atf_check -s not-exit:0 \
	    -e match:"^cp: /$dst: (Permission|Read-only)" \
	    cp "$dst" /
	atf_check -s not-exit:0 \
	    -e match:"^cp: /$dst: (Permission|Read-only)" \
	    cp "$dst" //
}
to_root_cleanup()
{
	(dst=$(cat dst) && rm "/$dst") 2>/dev/null || true
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case basic_symlink
	atf_add_test_case chrdev
	atf_add_test_case hardlink
	atf_add_test_case hardlink_exists
	atf_add_test_case hardlink_exists_force
	atf_add_test_case matching_srctgt
	atf_add_test_case matching_srctgt_contained
	atf_add_test_case matching_srctgt_link
	atf_add_test_case matching_srctgt_nonexistent
	atf_add_test_case pflag_acls
	atf_add_test_case pflag_flags
	atf_add_test_case recursive_link_dflt
	atf_add_test_case recursive_link_Hflag
	atf_add_test_case recursive_link_Lflag
	atf_add_test_case samefile
	atf_add_test_case sparse_leading_hole
	atf_add_test_case sparse_multiple_holes
	atf_add_test_case sparse_only_hole
	atf_add_test_case sparse_to_dev
	atf_add_test_case sparse_trailing_hole
	atf_add_test_case standalone_Pflag
	atf_add_test_case symlink
	atf_add_test_case symlink_exists
	atf_add_test_case symlink_exists_force
	atf_add_test_case directory_to_symlink
	atf_add_test_case overwrite_directory
	atf_add_test_case to_dir_dne
	atf_add_test_case to_nondir
	atf_add_test_case to_deadlink
	atf_add_test_case to_deadlink_append
	atf_add_test_case to_dirlink
	atf_add_test_case to_deaddirlink
	atf_add_test_case to_link_outside
	atf_add_test_case dstmode
	atf_add_test_case to_root
}
