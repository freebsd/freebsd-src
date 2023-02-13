#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2022 Kyle Evans <kevans@FreeBSD.org>
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

atf_test_case sum_bflag
sum_bflag_body()
{
	cp $(atf_get_srcdir)/sum_a.in a
	cp $(atf_get_srcdir)/sum_a.in b

	(sha256 -q a | tr -d '\n'; echo " *a") > expected
	(sha256 -q b | tr -d '\n'; echo " *b") >> expected

	atf_check -o file:expected sha256sum -b a b
}

atf_test_case sum_cflag
sum_cflag_body()
{

	# Verify that the *sum -c mode works even if some files are missing.
	# PR 267722 identified that we would never advance past the first record
	# to check against.  As a result, things like checking the published
	# checksums for the install media became a more manual process again if
	# you didn't download all of the images.
	for combo in "a b c" "b c" "a c" "a b" "a" "b" "c" ""; do
		rm -f a b c
		:> out
		cnt=0
		for f in ${combo}; do
			cp $(atf_get_srcdir)/sum_${f}.in ${f}
			printf "${f}: OK\n" >> out
			cnt=$((cnt + 1))
		done

		err=0
		[ "$cnt" -eq 3 ] || err=1
		atf_check -o file:out -e ignore -s exit:${err} \
		    sha256sum -c $(atf_get_srcdir)/sum_sums.digest
	done

}

atf_test_case sum_tflag
sum_tflag_body()
{
	cp $(atf_get_srcdir)/sum_a.in a

	# -t is a nop, not a time trial, when used with the *sum versions
	(sha256 -q a | tr -d '\n'; echo "  a") > expected
	atf_check -o file:expected sha256sum -t a
}

atf_init_test_cases()
{
	atf_add_test_case sum_bflag
	atf_add_test_case sum_cflag
	atf_add_test_case sum_tflag
}
