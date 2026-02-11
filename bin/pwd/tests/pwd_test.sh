#
# Copyright (c) 2026 Dag-Erling Smørgrav
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case logical
logical_head()
{
	atf_set "descr" "Cases where pwd prints the " \
		"logical working directory"
}
logical_body()
{
	root=$(realpath $PWD)
	atf_check mkdir -p phy/baz
	atf_check ln -s phy log
	cd log/baz

	# explicitly request logical
	export PWD="$root/log/baz"
	atf_check -o inline:"$root/log/baz\n" pwd -L
	atf_check -o inline:"$root/log/baz\n" pwd -P -L

	# logical is also the default
	export PWD="$root/log/baz"
	atf_check -o inline:"$root/log/baz\n" pwd
}

atf_test_case physical
physical_head()
{
	atf_set "descr" "Cases where pwd prints the " \
		"physical working directory"
}
physical_body()
{
	root=$(realpath $PWD)
	atf_check mkdir -p phy/baz
	atf_check ln -s phy log
	cd log/baz

	# explicitly request physical
	export PWD="$root/log/baz"
	atf_check -o inline:"$root/phy/baz\n" pwd -P
	atf_check -o inline:"$root/phy/baz\n" pwd -L -P

	# request logical but $PWD is relative
	export PWD="log/baz"
	atf_check -o inline:"$root/phy/baz\n" pwd -L

	# request logical but $PWD contains dot
	export PWD="$root/log/./baz"
	atf_check -o inline:"$root/phy/baz\n" pwd -L

	# request logical but $PWD contains dot-dot
	export PWD="$root/log/../log/baz"
	atf_check -o inline:"$root/phy/baz\n" pwd -L

	# request logical but $PWD does not exist
	export PWD="$root/baz"
	atf_check -o inline:"$root/phy/baz\n" pwd -L

	# request logical but $PWD does not match
	export PWD="$root/log"
	atf_check -o inline:"$root/phy/baz\n" pwd -L
}

atf_test_case stdout
stdout_head()
{
	atf_set descr "error writing to stdout"
}
stdout_body()
{
	pwd=$(which pwd)
	[ -f "$pwd" ] || atf_skip "unable to distinguish binary from builtin"
	(
		trap "" PIPE
		# Give true(1) some time to exit.
		sleep 1
		$pwd 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"1\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_init_test_cases()
{
	atf_add_test_case logical
	atf_add_test_case physical
	atf_add_test_case stdout
}
