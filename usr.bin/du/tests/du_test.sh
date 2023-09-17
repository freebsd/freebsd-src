#
# Copyright (c) 2017 Enji Cooper <ngie@FreeBSD.org>
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

require_sparse_file_support()
{
	if ! getconf MIN_HOLE_SIZE "$(pwd)"; then
		echo "getconf MIN_HOLE_SIZE $(pwd) failed; sparse files " \
		    "probably not supported by file system"
		mount
		atf_skip "Test's work directory does not support sparse files;" \
		    "try with a different TMPDIR?"
	fi
}

atf_test_case A_flag
A_flag_head()
{
	atf_set "descr" "Verify -A behavior"
}
A_flag_body()
{
	require_sparse_file_support
	# XXX: compressed volumes?
	atf_check truncate -s 10g sparse.file
	atf_check -o inline:'1\tsparse.file\n' du -g sparse.file
	atf_check -o inline:'10\tsparse.file\n' du -A -g sparse.file
}

atf_test_case H_flag
H_flag_head()
{
	atf_set "descr" "Verify -H behavior"
}
H_flag_body()
{
	local paths1='testdir/A/B testdir/A testdir/C testdir'
	local paths2='testdir/C/B testdir/C'
	local lineprefix=$'^[0-9]+\t'
	local sep="\$\n${lineprefix}"

	atf_check mkdir testdir
	atf_check -x "cd testdir && mkdir A && touch A/B && ln -s A C"

	atf_check -o save:du.out du -aAH testdir
	atf_check egrep -q "${lineprefix}$(echo $paths1 | sed -e "s/ /$sep/g")$" du.out
	# Check that the output doesn't contain any lines (i.e. paths) that we
	# did not expect it to contain from $paths1.
	atf_check -s exit:1 egrep -vq "${lineprefix}$(echo $paths1 | sed -e "s/ /$sep/g")$" du.out

	atf_check -o save:du_C.out du -aAH testdir/C
	atf_check egrep -q "${lineprefix}$(echo $paths2 | sed -e "s/ /$sep/g")$" du_C.out

	# Check that the output doesn't contain any lines (i.e. paths) that we
	# did not expect it to contain from $paths2.
	atf_check -s exit:1 egrep -vq "${lineprefix}$(echo $paths2 | sed -e "s/ /$sep/g")$" du_C.out
}

atf_test_case I_flag
I_flag_head()
{
	atf_set "descr" "Verify -I behavior"
}
I_flag_body()
{
	paths_sans_foo_named="a/motley/fool/of/sorts fool/parts/with/their/cache bar baz"
	paths_foo_named="foo foobar"
	paths="$paths_sans_foo_named $paths_foo_named"

	# cd'ing to testdir helps ensure that files from atf/kyua don't
	# pollute the results.
	atf_check -x "mkdir testdir && cd testdir && mkdir -p $paths"
	atf_check -o save:du.out -x "cd testdir && du -s $paths_sans_foo_named"
	atf_check -o save:du_I.out -x "cd testdir && du -I '*foo*' -s $paths"

	atf_check diff -u du.out du_I.out
}

atf_test_case c_flag
c_flag_head()
{
	atf_set	"descr" "Verify -c output"
}
c_flag_body()
{
	atf_check truncate -s 0 foo bar
}

atf_test_case g_flag
g_flag_head()
{
	atf_set "descr" "Verify -g output"
}
g_flag_body()
{
	require_sparse_file_support
	atf_check truncate -s 1k A
	atf_check truncate -s 1m B
	atf_check truncate -s 1g C
	atf_check truncate -s 1t D
	atf_check -o inline:'1\tA\n1\tB\n1\tC\n1024\tD\n' du -Ag A B C D
}

atf_test_case h_flag
h_flag_head()
{
	atf_set	"descr" "Verify -h output"
}
h_flag_body()
{
	require_sparse_file_support
	atf_check truncate -s 1k A
	atf_check truncate -s 1m B
	atf_check truncate -s 1g C
	atf_check truncate -s 1t D
	atf_check -o inline:'1.0K\tA\n1.0M\tB\n1.0G\tC\n1.0T\tD\n' du -Ah A B C D
}

atf_test_case k_flag
k_flag_head()
{
	atf_set "descr" "Verify -k output"
}
k_flag_body()
{
	atf_check truncate -s 1k A
	atf_check truncate -s 1m B
	atf_check -o inline:'1\tA\n1024\tB\n' du -Ak A B
}

atf_test_case m_flag
m_flag_head()
{
	atf_set "descr" "Verify -m output"
}
m_flag_body()
{
	atf_check truncate -s 1k A
	atf_check truncate -s 1m B
	atf_check truncate -s 1g C
	atf_check -o inline:'1\tA\n1\tB\n1024\tC\n' du -Am A B C
}

atf_test_case si_flag
si_flag_head()
{
	atf_set "descr" "Verify --si output"
}
si_flag_body()
{
	atf_check truncate -s 1500000 A
	atf_check truncate -s 1572864 B

	atf_check -o inline:'1.4M\tA\n1.5M\tB\n' du -Ah A B
	atf_check -o inline:'1.5M\tA\n1.6M\tB\n' du -A --si A B
}

atf_init_test_cases()
{
	atf_add_test_case A_flag
	atf_add_test_case H_flag
	atf_add_test_case I_flag
	atf_add_test_case g_flag
	atf_add_test_case h_flag
	atf_add_test_case k_flag
	atf_add_test_case m_flag
	atf_add_test_case si_flag
}
