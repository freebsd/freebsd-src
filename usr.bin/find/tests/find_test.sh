#-
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2017, Conrad Meyer <cem@FreeBSD.org>.
# Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

atf_test_case find_newer_link
find_newer_link_head()
{
	atf_set "descr" "Verifies that -newer correctly uses a symlink, " \
	    "rather than its target, for comparison"
}
find_newer_link_body()
{
	atf_check -s exit:0 mkdir test
	atf_check -s exit:0 ln -s file1 test/link
	atf_check -s exit:0 touch -d 2017-12-31T10:00:00Z -h test/link
	atf_check -s exit:0 touch -d 2017-12-31T11:00:00Z test/file2
	atf_check -s exit:0 touch -d 2017-12-31T12:00:00Z test/file1

	# find(1) should evaluate 'link' as a symlink rather than its target
	# (with -P / without -L flags).  Since link was created first, the
	# other two files should be newer.
	echo -e "test\ntest/file1\ntest/file2" > expout
	atf_check -s exit:0 -o save:output find test -newer test/link
	atf_check -s exit:0 -o file:expout sort < output
}

atf_test_case find_samefile_link
find_samefile_link_head()
{
	atf_set "descr" "Verifies that -samefile correctly uses a symlink, " \
	    "rather than its target, for comparison"
}
find_samefile_link_body()
{
	atf_check -s exit:0 mkdir test
	atf_check -s exit:0 touch test/file3
	atf_check -s exit:0 ln -s file3 test/link2

	# find(1) should evaluate 'link' as a symlink rather than its target
	# (with -P / without -L flags).
	atf_check -s exit:0 -o "inline:test/link2\n" find test -samefile test/link2
}

atf_test_case find_printf
find_printf_head()
{
	atf_set "descr" "Test the -printf primary"
}
find_printf_body()
{
	mkdir dir
	chmod 0755 dir
	jot -b hello 1024 >dir/file
	chmod 0644 dir/file
	ln -s file dir/link
	chmod -h 0444 dir/link
	local db=$(stat -f %b dir)
	local fb=$(stat -f %b dir/file)
	local lb=$(stat -f %b dir/link)

	# paths
	atf_check -o inline:"dir\ndir/file\ndir/link\n" \
	    find -s dir -printf '%p\n'
	atf_check -o inline:"dir\nfile\nlink\n" \
	    find -s dir -printf '%f\n'
	atf_check -o inline:".\ndir\ndir\n" \
	    find -s dir -printf '%h\n'
	atf_check -s exit:1 -e match:"unimplemented" -o ignore \
	    find -s dir -printf '%P\n'
	atf_check -s exit:1 -e match:"unimplemented" -o ignore \
	    find -s dir -printf '%H\n'

	# group
	atf_check -o inline:"$(stat -f %Sg dir dir/file dir/link)\n" \
	    find -s dir -printf '%g\n'
	atf_check -o inline:"$(stat -f %g dir dir/file dir/link)\n" \
	    find -s dir -printf '%G\n'

	# owner
	atf_check -o inline:"$(stat -f %Su dir dir/file dir/link)\n" \
	    find -s dir -printf '%u\n'
	atf_check -o inline:"$(stat -f %u dir dir/file dir/link)\n" \
	    find -s dir -printf '%U\n'

	# mode
	atf_check -o inline:"$(stat -f %Lp dir dir/file dir/link)\n" \
	    find -s dir -printf '%m\n'
	atf_check -o inline:"$(stat -f %Sp dir dir/file dir/link)\n" \
	    find -s dir -printf '%M\n'

	# size
	atf_check -o inline:"$((db/2))\n$((fb/2))\n$((lb/2))\n" \
	    find -s dir -printf '%k\n'
	atf_check -o inline:"$db\n$fb\n$lb\n" \
	    find -s dir -printf '%b\n'
	atf_check -o inline:"$(stat -f %z dir dir/file dir/link)\n" \
	    find -s dir -printf '%s\n'
	# XXX test %S properly
	atf_check -o ignore \
	    find -s dir -printf '%S\n'
	atf_check -o inline:"0\n1\n1\n" \
	    find -s dir -printf '%d\n'

	# device
	atf_check -o inline:"$(stat -f %d dir dir/file dir/link)\n" \
	    find -s dir -printf '%D\n'
	atf_check -s exit:1 -e match:"unimplemented" -o ignore \
	    find -s dir -printf '%F\n'

	# link target
	atf_check -o inline:"\n\nfile\n" \
	    find -s dir -printf '%l\n'

	# inode
	atf_check -o inline:"$(stat -f %i dir dir/file dir/link)\n" \
	    find -s dir -printf '%i\n'

	# nlinks
	atf_check -o inline:"$(stat -f %l dir dir/file dir/link)\n" \
	    find -s dir -printf '%n\n'

	# type
	atf_check -o inline:"d\nf\nl\n" \
	    find -s dir -printf '%y\n'
	atf_check -o inline:"d\nf\nf\n" \
	    find -s dir -printf '%Y\n'

	# access time
	atf_check -o inline:"$(stat -f %Sa -t '%a %b %e %T %Y' dir dir/file dir/link)\n" \
	    find -s dir -printf '%a\n'
	atf_check -o inline:"$(stat -f %Sa -t '%e' dir dir/file dir/link)\n" \
	    find -s dir -printf '%Ae\n'

	# birth time
	atf_check -o inline:"$(stat -f %SB -t '%e' dir dir/file dir/link)\n" \
	    find -s dir -printf '%Be\n'

	# inode change time
	atf_check -o inline:"$(stat -f %Sc -t '%a %b %e %T %Y' dir dir/file dir/link)\n" \
	    find -s dir -printf '%c\n'
	atf_check -o inline:"$(stat -f %Sc -t '%e' dir dir/file dir/link)\n" \
	    find -s dir -printf '%Ce\n'

	# modification time
	atf_check -o inline:"$(stat -f %Sm -t '%a %b %e %T %Y' dir dir/file dir/link)\n" \
	    find -s dir -printf '%t\n'
	atf_check -o inline:"$(stat -f %Sm -t '%e' dir dir/file dir/link)\n" \
	    find -s dir -printf '%Te\n'
}

atf_init_test_cases()
{
	atf_add_test_case find_newer_link
	atf_add_test_case find_samefile_link
	atf_add_test_case find_printf
}
