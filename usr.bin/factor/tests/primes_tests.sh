#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright 2023 (C) Enji Cooper
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

atf_test_case invalid_range
invalid_range_head()
{
	atf_set "descr" "Verify that invalid ranges are not allowed."
}
invalid_range_body()
{
	atf_check -s not-exit:0 -e inline:"primes: start value must be less than stop value.\n" primes -- 2 1
}

atf_test_case negative_numbers_not_allowed
negative_numbers_not_allowed_head()
{
	atf_set "descr" "Verify that negative numbers are not allowed."
}
negative_numbers_not_allowed_body()
{
	atf_check -s not-exit:0 -e inline:"primes: negative numbers aren't permitted.\n" primes -- -1 0
}

atf_test_case no_primes_between_between_20_and_22
no_primes_between_between_20_and_22_head()
{
	atf_set "descr" "Show that no primes exist between [20, 22]."
}

no_primes_between_between_20_and_22_body()
{
	atf_check primes 20 22
}

atf_test_case primes_in_20_to_50_range
primes_in_20_to_50_range_head()
{
	atf_set "descr" "Find all primes between [20, 50]."
}

primes_in_20_to_50_range_body()
{
	atf_check -o inline:"23\n29\n31\n37\n41\n43\n47\n" primes 20 50
}

atf_init_test_cases()
{
	atf_add_test_case invalid_range
	atf_add_test_case negative_numbers_not_allowed
	atf_add_test_case no_primes_between_between_20_and_22
	atf_add_test_case primes_in_20_to_50_range
}
