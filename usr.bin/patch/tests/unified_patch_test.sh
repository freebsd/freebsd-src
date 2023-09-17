#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2019 Kyle Evans <kevans@FreeBSD.org>
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

atf_test_case basic
basic_body()
{
	printf "a\nb\nc\nd\ne\nf\ng\nh\ni\n" > foo_full
	printf "a\nb\nc\n" > foo_start
	printf "g\nh\ni\n" > foo_end
	printf "d\ne\nf\n" > foo_middle

	diff -u foo_start foo_full > foo_start2full.diff
	diff -u foo_end foo_full > foo_end2full.diff
	diff -u foo_middle foo_full > foo_mid2full.diff

	# Check lengths... each should have all 9 lines + 3 line header
	atf_check -o inline:"12" -x \
	    "cat foo_start2full.diff | wc -l | tr -d '[:space:]'"
	atf_check -o inline:"12" -x \
	    "cat foo_end2full.diff | wc -l | tr -d '[:space:]'"
	atf_check -o inline:"12" -x \
	    "cat foo_mid2full.diff | wc -l | tr -d '[:space:]'"

	# Apply the patch! Should succeed
	atf_check -o ignore patch foo_start foo_start2full.diff \
	    -o foo_start2full
	atf_check -o ignore patch foo_end foo_end2full.diff \
	    -o foo_end2full
	atf_check -o ignore patch foo_middle foo_mid2full.diff \
	    -o foo_mid2full

	# And these should all produce equivalent to the original full
	atf_check -o ignore diff foo_start2full foo_full
	atf_check -o ignore diff foo_end2full foo_full
	atf_check -o ignore diff foo_mid2full foo_full
}

atf_test_case limited_ctx
limited_ctx_head()
{
	atf_set "descr" "Verify correct behavior with limited context (PR 74127)"
}
limited_ctx_body()
{

	# First; PR74127-repro.diff should not have applied, but it instead
	# assumed a match and added the modified line at the offset specified...
	atf_check -s not-exit:0 -o ignore -e ignore patch -o _.out \
	    "$(atf_get_srcdir)/PR74127.in" \
	    "$(atf_get_srcdir)/PR74127-repro.diff"

	# Let's extend that and make sure a similarly ill-contexted diff does
	# not apply even with the correct line number
	atf_check -s not-exit:0 -o ignore -e ignore patch -o _.out \
	    "$(atf_get_srcdir)/PR74127.in" \
	    "$(atf_get_srcdir)/PR74127-line.diff"

	# Correct line number and correct old line should always work
	atf_check -o ignore -e ignore patch -o _.out \
	    "$(atf_get_srcdir)/PR74127.in" \
	    "$(atf_get_srcdir)/PR74127-good.diff"
}

atf_test_case file_creation
file_creation_body()
{

	echo "x" > foo
	diff -u /dev/null foo > foo.diff
	rm foo

	atf_check -x "patch -s < foo.diff"
	atf_check -o ignore stat foo
}

# This test is motivated by long-standing bugs that occasionally slip by in
# commits.  If a file is created by a diff, patch(1) will happily duplicate the
# contents as many times as you apply the diff.  It should instead detect that
# a source of /dev/null creates the file, so it shouldn't exist.  Furthermore,
# the reverse of creation is deletion -- hence the next test, which ensures that
# the file is removed if it's empty once the patch is reversed.  The size checks
# are scattered throughout to make sure that we didn't get some kind of false
# error, and the first size check is merely a sanity check that should be
# trivially true as this is executed in a sandbox.
atf_test_case file_nodupe
file_nodupe_body()
{

	echo "x" > foo
	diff -u /dev/null foo > foo.diff

	atf_check -o inline:"2\n" stat -f "%z" foo
	atf_check -s not-exit:0 -o ignore -x "patch -Ns < foo.diff"
	atf_check -o inline:"2\n" stat -f "%z" foo
	atf_check -s not-exit:0 -o ignore -x "patch -fs < foo.diff"
	atf_check -o inline:"2\n" stat -f "%z" foo
}

atf_test_case file_removal
file_removal_body()
{

	echo "x" > foo
	diff -u /dev/null foo > foo.diff

	# Check that the file is removed completely if it was sourced from
	# /dev/null
	atf_check -x "patch -Rs < foo.diff"
	atf_check -s not-exit:0 -e ignore stat foo

	# But if it had been modified, we'll only remove the portion that the
	# patch would have created.  This makes us compatible with GNU patch's
	# behavior, at least.  Whether that is the sane action or not is a
	# question for further study, and then this comment may be removed.
	printf "x\ny\n" > foo
	atf_check -x "patch -Rs < foo.diff"
	atf_check -o inline:"y\n" cat foo
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case limited_ctx
	atf_add_test_case file_creation
	atf_add_test_case file_nodupe
	atf_add_test_case file_removal
}
