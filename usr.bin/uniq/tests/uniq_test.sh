#
# Copyright (c) 2024 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_check_uniq() {
	atf_check uniq "$@" input actual
	atf_check diff -u actual expected
	atf_check uniq "$@" - actual <input
	atf_check diff -u actual expected
	atf_check -o file:expected uniq "$@" input
	atf_check -o file:expected uniq "$@" <input
	atf_check -o file:expected uniq "$@" - <input
}

atf_test_case basic
basic_head() {
	atf_set descr "basic test without options"
}
basic_body() {
	printf "a\na\nb\nb\na\na\n" >input
	printf "a\nb\na\n" >expected
	atf_check_uniq
}

atf_test_case count
count_head() {
	atf_set descr "basic test showing counts"
}
count_body() {
	printf "a\na\nb\nb\nb\na\na\na\na\n" >input
	printf "   2 a\n   3 b\n   4 a\n" >expected
	atf_check_uniq -c
	atf_check_uniq --count
}

atf_test_case repeated
repeated_head() {
	atf_set descr "print repeated lines only"
}
repeated_body() {
	printf "a\na\nb\na\na\n" >input
	printf "a\na\n" >expected
	atf_check_uniq -d
	atf_check_uniq --repeated
}

atf_test_case count_repeated
count_repeated_head() {
	atf_set descr "count and print repeated lines only"
}
count_repeated_body() {
	printf "a\na\nb\nb\na\n" >input
	printf "   2 a\n   2 b\n" >expected
	atf_check_uniq --count --repeated
}

atf_test_case all_repeated
all_repeated_head() {
	atf_set descr "print every instance of repeated lines"
}
all_repeated_body() {
	printf "a\na\nb\na\na\n" >input
	printf "a\na\na\na\n" >expected
	atf_check_uniq -D
	atf_check_uniq --all-repeated
}

atf_test_case skip_fields
skip_fields_head() {
	atf_set descr "skip fields"
}
skip_fields_body() {
	printf "1 a\n2 a\n3 b\n4 b\n5 a\n6 a\n" >input
	printf "1 a\n3 b\n5 a\n" >expected
	atf_check_uniq -f 1
	atf_check_uniq --skip-fields 1
}

atf_test_case skip_fields_tab
skip_fields_tab_head() {
	atf_set descr "skip fields (with tabs)"
}
skip_fields_tab_body() {
	printf "1\ta\n2\ta\n3\tb\n4\tb\n5\ta\n6\ta\n" >input
	printf "1\ta\n3\tb\n5\ta\n" >expected
	atf_check_uniq -f 1
	atf_check_uniq --skip-fields 1
}

atf_test_case ignore_case
ignore_case_head() {
	atf_set descr "ignore case"
}
ignore_case_body() {
	printf "a\nA\nb\nB\na\nA\n" >input
	printf "a\nb\na\n" >expected
	atf_check_uniq -i
	atf_check_uniq --ignore-case
}

atf_test_case skip_chars
skip_chars_head() {
	atf_set descr "skip chars"
}
skip_chars_body() {
	printf "1 a\n2 a\n3 b\n4 b\n5 a\n6 a\n" >input
	printf "1 a\n3 b\n5 a\n" >expected
	atf_check_uniq -s 2
	atf_check_uniq --skip-chars 2
}

atf_test_case unique
unique_head() {
	atf_set descr "print non-repeated lines only"
}
unique_body() {
	printf "a\na\nb\na\na\n" >input
	printf "b\n" >expected
	atf_check_uniq -u
	atf_check_uniq --unique
}

atf_test_case count_unique
count_unique_head() {
	atf_set descr "print non-repeated lines with count"
}
count_unique_body() {
	printf "a\na\nb\n" >input
	printf "   1 b\n" >expected
	atf_check_uniq --unique --count
	atf_check_uniq --count --unique
}

atf_test_case interactive
interactive_head() {
	atf_set descr "test interactive use"
}
interactive_body() {
	sh -c 'yes | stdbuf -oL uniq >actual' &
	pid=$!
	sleep 1
	kill $!
	atf_check -o inline:"y\n" cat actual
}

atf_test_case interactive_repeated
interactive_repeated_head() {
	atf_set descr "test interactive use with -d"
}
interactive_repeated_body() {
	sh -c 'yes | stdbuf -oL uniq -d >actual' &
	pid=$!
	sleep 1
	kill $!
	atf_check -o inline:"y\n" cat actual
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case count
	atf_add_test_case repeated
	atf_add_test_case count_repeated
	atf_add_test_case all_repeated
	atf_add_test_case skip_fields
	atf_add_test_case skip_fields_tab
	atf_add_test_case ignore_case
	atf_add_test_case skip_chars
	atf_add_test_case unique
	atf_add_test_case count_unique
	atf_add_test_case interactive
	atf_add_test_case interactive_repeated
}
