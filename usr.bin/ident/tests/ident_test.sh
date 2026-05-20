#
# Copyright (c) 2026 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case stdin
stdin_head()
{
	atf_set "descr" "Read from stdin"
}
stdin_body()
{
	local dir=$(atf_get_srcdir)
	atf_check -o file:"${dir}/test.out" \
		ident < "${dir}/test.in"
}

atf_test_case file
file_head()
{
	atf_set "descr" "Read from a file"
}
file_body()
{
	local dir=$(atf_get_srcdir)
	echo "${dir}/test.in:" >out
	cat "${dir}/test.out" >>out
	atf_check -o file:out ident "${dir}/test.in"
}

atf_test_case noid
noid_head()
{
	atf_set "descr" "No id keywords in input"
}
noid_body()
{
	local dir=$(atf_get_srcdir)
	atf_check \
	    -s exit:1 \
	    -o inline:"${dir}/testnoid:\n" \
	    -e inline:"ident warning: no id keywords in ${dir}/testnoid\n" \
	    ident "${dir}/testnoid"
}

atf_test_case multi
multi_head()
{
	atf_set "descr" "Multiple inputs"
}
multi_body()
{
	local dir=$(atf_get_srcdir)
	echo "${dir}/test.in:" >out
	cat "${dir}/test.out" >>out
	echo "${dir}/testnoid:" >>out
	atf_check \
	    -s exit:1 \
	    -o file:out \
	    -e inline:"ident warning: no id keywords in ${dir}/testnoid\n" \
	    ident "${file}"
}

atf_test_case stdout
stdout_head()
{
	atf_set "descr" "Failure to write to stdout"
}
stdout_body()
{
	local dir=$(atf_get_srcdir)
	(
		trap "" PIPE
		sleep 1
		ident "${dir}"/test.in 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"1\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_init_test_cases()
{
	atf_add_test_case stdin
	atf_add_test_case file
	atf_add_test_case noid
	atf_add_test_case stdout
}
