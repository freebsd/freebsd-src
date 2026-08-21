#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Baptiste Daroussin <bapt@FreeBSD.org>
#
#

atf_test_case E_output
E_output_body()
{
	printf 'a\tb\n\001c\n\177\n' > input
	atf_check -o 'inline:a\tb$\n\001c$\n\0177$\n' cat -E input
}

atf_test_case T_output
T_output_body()
{
	printf 'a\tb\n\001c\n\177\n' > input
	atf_check -o 'inline:a^Ib\n\001c\n\0177\n' cat -T input
}

atf_test_case A_output
A_output_body()
{
	printf 'a\tb\n\001c\n\177\n' > input

	# -A implies -v, -E and -T.
	expected='a^Ib$\n^Ac$\n^?$\n'
	atf_check -o "inline:${expected}" cat -A input
	atf_check -o "inline:${expected}" cat -vET input
	atf_check -o "inline:${expected}" cat -AET input
}

atf_test_case e_implies_v
e_implies_v_body()
{
	printf 'a\tb\n\001c\n\177\n' > input

	atf_check -o 'inline:a\tb$\n^Ac$\n^?$\n' cat -e input
	atf_check -o 'inline:a\tb$\n^Ac$\n^?$\n' cat -Ev input
}

atf_test_case t_implies_v
t_implies_v_body()
{
	printf 'a\tb\n\001c\n\177\n' > input

	atf_check -o 'inline:a^Ib\n^Ac\n^?\n' cat -t input
	atf_check -o 'inline:a^Ib\n^Ac\n^?\n' cat -Tv input
}

atf_init_test_cases()
{
	atf_add_test_case E_output
	atf_add_test_case T_output
	atf_add_test_case A_output
	atf_add_test_case e_implies_v
	atf_add_test_case t_implies_v
}
