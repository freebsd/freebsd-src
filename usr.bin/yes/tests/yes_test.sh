#
# Copyright (c) 2026 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case none
none_head()
{
	atf_set "descr" "No arguments"
}
none_body()
{
	atf_check \
	    -o inline:"y\ny\ny\ny\ny\n" \
	    -x "yes | head -5"
}

atf_test_case one
one_head()
{
	atf_set "descr" "One argument"
}
one_body()
{
	local y="Hello, world!"
	atf_check \
	    -o inline:"${y}\n${y}\n${y}\n${y}\n${y}\n" \
	    -x "yes '${y}' | head -5"
}

atf_test_case multi
multi_head()
{
	atf_set "descr" "Multiple arguments"
}
multi_body()
{
	set -- The Magic Words are Squeamish Ossifrage
	local y="$*"
	atf_check \
	    -o inline:"${y}\n${y}\n${y}\n${y}\n${y}\n" \
	    -x "yes $* | head -5"
}

atf_test_case argv
argv_head()
{
	atf_set "descr" "Verify that argv is unmolested"
}
argv_body()
{
	yes y >/dev/null &
	local pid=$!
	# Wait for yes(1) to exec before checking args
	sleep 0.1
	atf_check -o inline:"yes y\n" ps -o args= $pid
	kill $pid
	wait
}

atf_test_case stdout
stdout_head()
{
	atf_set descr "Error writing to stdout"
}
stdout_body()
{
	(
		trap "" PIPE
		# Give true(1) some time to exit.
		sleep 1
		yes 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"1\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_init_test_cases()
{
	atf_add_test_case none
	atf_add_test_case one
	atf_add_test_case multi
	atf_add_test_case argv
	atf_add_test_case stdout
}
