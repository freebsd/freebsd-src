#
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case basic
basic_head()
{
	atf_set "descr" "Sort a basic graph"
}
basic_body()
{
	cat >input <<EOF
A B
A F
B C
B D
D E
EOF
	cat >output <<EOF
A
F
B
D
C
E
EOF
	atf_check -o file:output tsort input
	atf_check -o file:output tsort <input
}

atf_test_case cycle
cycle_head()
{
	atf_set "descr" "Sort a graph with a cycle"
}
cycle_body()
{
	cat >input <<EOF
A B
A F
B C
B D
D E
D A
EOF
	cat >output<<EOF
D
E
A
F
B
C
EOF
	atf_check -e match:cycle -o file:output tsort input
	atf_check -e match:cycle -o file:output tsort <input
	atf_check -o file:output tsort -q input
	atf_check -o file:output tsort -q <input
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case cycle
}
