#
# Copyright (c) 2025 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case basic
basic_head()
{
	atf_set "descr" "Test basic lam(1) functionality"
}
basic_body()
{
	printf '1\n2\n3\n' > a
	printf '4\n5\n6\n' > b

	atf_check -o inline:"14\n25\n36\n" lam a b
}

atf_test_case sep
sep_head()
{
	atf_set "descr" "Test lam(1) -s and -S options"
}
sep_body()
{
	printf "1\n" > a
	printf "0\n" > b

	atf_check -o inline:"x1x0\n" lam -S x a b
	atf_check -o inline:"1x0\n" lam a -S x b
	atf_check -o inline:"x10\n" lam -S x a -s '' b

	atf_check -o inline:"x10\n" lam -s x a b
	atf_check -o inline:"x1y0\n" lam -s x a -s y b
	atf_check -o inline:"1x0\n" lam a -s x b
}

atf_test_case stdin
stdin_head()
{
	atf_set "descr" "Test lam(1) using stdin"
}
stdin_body()
{
	printf '1\n2\n3\n4\n' > a

	atf_check -o inline:"11\n22\n33\n44\n" lam a - < a
	atf_check -o inline:"11\n22\n33\n44\n" lam - a < a

	atf_check -o inline:"12\n34\n" lam - - < a
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case sep
	atf_add_test_case stdin
}
