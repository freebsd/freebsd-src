#
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

a="The magic words are"
b="Squeamish Ossifrage"

atf_check_asa() {
	atf_check -o file:"$2" asa "$1"
	atf_check -o file:"$2" asa <"$1"
	atf_check -o file:"$2" asa - <"$1"
}

atf_test_case space
space_head() {
	atf_set descr "First character on line is ' '"
}
space_body() {
	printf " %s\n %s\n" "$a" "$b" >infile
	printf "%s\n%s\n" "$a" "$b" >outfile
	atf_check_asa infile outfile
}

atf_test_case zero
zero_head() {
	atf_set descr "First character on line is '0'"
}
zero_body() {
	printf " %s\n0%s\n" "$a" "$b" >infile
	printf "%s\n\n%s\n" "$a" "$b" >outfile
	atf_check_asa infile outfile
}

atf_test_case one
one_head() {
	atf_set descr "First character on line is '1'"
}
one_body() {
	printf "1%s\n1%s\n" "$a" "$b" >infile
	printf "\f%s\n\f%s\n" "$a" "$b" >outfile
	atf_check_asa infile outfile
}

atf_test_case plus
plus_head() {
	atf_set descr "First character on line is '+'"
}
plus_body() {
	printf " %s\n+%s\n" "$a" "$b" >infile
	printf "%s\r%s\n" "$a" "$b" >outfile
	atf_check_asa infile outfile
}

atf_test_case plus_top
plus_top_head() {
	atf_set descr "First character in input is '+'"
}
plus_top_body() {
	printf "+%s\n+%s\n" "$a" "$b" >infile
	printf "%s\r%s\n" "$a" "$b" >outfile
	atf_check_asa infile outfile
}

atf_test_case stdout
stdout_head() {
	atf_set descr "Failure to write to stdout"
}
stdout_body() {
	(
		trap "" PIPE
		sleep 1
		echo " $a $b" | asa 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"1\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_test_case dashdash
dashdash_head() {
	atf_set descr "Use -- to end options"
}
dashdash_body() {
	echo " $a $b" >-infile
	atf_check -s not-exit:0 -e match:"illegal option" asa -infile
	atf_check -o inline:"$a $b\n" asa -- -infile
}

atf_test_case unterminated
unterminated_head() {
	atf_set descr "Unterminated input"
}
unterminated_body() {
	printf " %s\n %s" "$a" "$b" >infile
	printf "%s\n%s" "$a" "$b" >outfile
	atf_check_asa infile outfile
}

atf_init_test_cases()
{
	atf_add_test_case space
	atf_add_test_case zero
	atf_add_test_case one
	atf_add_test_case plus
	atf_add_test_case plus_top
	atf_add_test_case stdout
	atf_add_test_case dashdash
	atf_add_test_case unterminated
}
