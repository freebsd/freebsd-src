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

atf_test_case float_non_prime
float_non_prime_head()
{
	atf_set "descr" "Test with a float non-prime number"
}
float_non_prime_body()
{
	atf_check -o inline:"1: 1\n" factor 1.44
}

atf_test_case float_prime
float_prime_head()
{
	atf_set "descr" "Test with a float prime number"
}
float_prime_body()
{
	pi="3.141592653589793238462643383279502884197"
	atf_check -o inline:"3: 3\n" factor $pi
}

atf_test_case int_non_prime
int_non_prime_head()
{
	atf_set "descr" "Test with an integral prime number"
}
int_non_prime_body()
{
	atf_check -o inline:"8: 2 2 2\n" factor 8
}

atf_test_case int_prime
int_prime_head()
{
	atf_set "descr" "Test with an integral prime number"
}
int_prime_body()
{
	atf_check -o inline:"31: 31\n" factor 31
}

atf_test_case negative_numbers_not_allowed
negative_numbers_not_allowed_head()
{
	atf_set "descr" "Verify that negative numbers are not allowed."
}
negative_numbers_not_allowed_body()
{
	atf_check -s not-exit:0 -e inline:"factor: negative numbers aren't permitted.\n" factor -- -4
}

atf_init_test_cases()
{
	atf_add_test_case float_non_prime
	atf_add_test_case float_prime
	atf_add_test_case int_non_prime
	atf_add_test_case int_prime
	atf_add_test_case negative_numbers_not_allowed
}
