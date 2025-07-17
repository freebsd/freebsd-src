#
# Copyright (c) 2024 Dag-Erling Smørgrav
#
# SPDX-License-Identifier: BSD-2-Clause
#

export TZ=UTC

atf_check_mtime()
{
	local mtime=$1 filename=$2
	atf_check -o inline:"$((mtime))\n" stat -f%m "$filename"
}

atf_test_case touch_none
touch_none_head()
{
	atf_set descr "No arguments"
}
touch_none_body()
{
	atf_check -s exit:1 -e match:"^usage" touch
}

atf_test_case touch_one
touch_one_head()
{
	atf_set descr "One argument"
}
touch_one_body()
{
	atf_check touch foo
	atf_check test -f foo
}

atf_test_case touch_multiple
touch_multiple_head()
{
	atf_set descr "Multiple arguments"
}
touch_multiple_body()
{
	atf_check touch foo bar baz
	atf_check test -f foo -a -f bar -a -f baz
}

atf_test_case touch_absolute
touch_absolute_head()
{
	atf_set descr "Absolute date / time"
}
touch_absolute_body()
{
	atf_check touch -t 7001010101 foo
	atf_check_mtime 3660 foo
	atf_check rm foo

	atf_check touch -t 7001010101.01 foo
	atf_check_mtime 3661 foo
	atf_check rm foo

	atf_check touch -t 196912312359 foo
	atf_check_mtime -60 foo
	atf_check rm foo

	atf_check touch -t 196912312359.58 foo
	atf_check_mtime -2 foo
	atf_check rm foo

	atf_check touch -t 196912312359.59 foo
	atf_expect_fail "VFS interprets -1 as “do not set”"
	atf_check_mtime -1 foo
	atf_check rm foo

	atf_check touch -d1969-12-31T23:59:58 foo
	atf_check_mtime -2 foo
	atf_check rm foo

	atf_check touch -d1969-12-31\ 23:59:58 foo
	atf_check_mtime -2 foo
	atf_check rm foo

	atf_check env TZ=CET touch -d1970-01-01T00:59:58 foo
	atf_check_mtime -2 foo
	atf_check rm foo

	atf_check env TZ=CET touch -d1970-01-01T00:59:58Z foo
	atf_check_mtime 3598 foo
	atf_check rm foo

	atf_check touch -d1969-12-31T23:59:59Z foo
	atf_expect_fail "VFS interprets -1 as “do not set”"
	atf_check_mtime -1 foo
	atf_check rm foo
}

atf_test_case touch_relative
touch_relative_head()
{
	atf_set descr "Relative date / time"
}
touch_relative_body()
{
	atf_check touch -t 202403241234.56 foo
	atf_check_mtime 1711283696 foo
	atf_check touch -A -36 foo
	atf_check_mtime 1711283660 foo
	atf_check touch -A -0100 foo
	atf_check_mtime 1711283600 foo
	atf_check touch -A -010000 foo
	atf_check_mtime 1711280000 foo
	atf_check touch -A 010136 foo
	atf_check_mtime 1711283696 foo
}

atf_test_case touch_copy
touch_copy_head()
{
	atf_set descr "Copy time from another file"
}
touch_copy_body()
{
	atf_check touch -t 202403241234.56 foo
	atf_check_mtime 1711283696 foo
	atf_check touch -t 7001010000 bar
	atf_check_mtime 0 bar
	atf_check touch -r foo bar
	atf_check_mtime 1711283696 bar
}

atf_test_case touch_nocreate
touch_nocreate_head()
{
	atf_set descr "Do not create file"
}
touch_nocreate_body()
{
	atf_check touch -t 202403241234.56 foo
	atf_check_mtime 1711283696 foo
	atf_check touch -c -t 7001010000 foo bar
	atf_check_mtime 0 foo
	atf_check -s exit:1 test -f bar
	atf_check touch -c bar
	atf_check -s exit:1 test -f bar
}

atf_init_test_cases()
{
	atf_add_test_case touch_none
	atf_add_test_case touch_one
	atf_add_test_case touch_multiple
	atf_add_test_case touch_absolute
	atf_add_test_case touch_relative
	atf_add_test_case touch_copy
	atf_add_test_case touch_nocreate
	# TODO: add test cases for -a, -h, -m
}
