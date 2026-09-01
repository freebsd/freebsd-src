#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Devin Teske <dteske@FreeBSD.org>
#

# The system calls a program makes vary with the machine and with the
# run-time linker, so these tests assert which system calls -t may and
# may not report rather than the exact sequence of them.

require_truss()
{
	truss -o /dev/null /usr/bin/true >/dev/null 2>&1 ||
	    atf_skip "unable to trace a child process here"
}

# Write the sorted, unique names of the system calls reported in a truss
# output file to another file.
syscall_names()
{
	sed -n 's/^\([a-zA-Z_][a-zA-Z0-9_.]*\)(.*$/\1/p' "$1" | sort -u > "$2"
}

# Fail unless every name in a file matches an extended regular expression.
only_names()
{
	if grep -Ev "$1" "$2" > unexpected; then
		atf_fail "reported system calls not selected by the filter:" \
		    "$(tr '\n' ' ' < unexpected)"
	fi
}

# Fail if any name in a file matches an extended regular expression.
no_name()
{
	if grep -E "$1" "$2" > unexpected; then
		atf_fail "system calls excluded by the filter were reported:" \
		    "$(tr '\n' ' ' < unexpected)"
	fi
}

# Fail unless at least one name in a file matches.
some_name()
{
	grep -Eq "$1" "$2" ||
	    atf_fail "no system call matching $1 was reported"
}

atf_test_case list
list_head()
{
	atf_set descr "-t with no expression prints the available groups"
}
list_body()
{
	atf_check -s exit:2 -o match:'@all' -o match:'@none' \
	    -o match:'@read' -o match:'@write' -o match:'@file' \
	    -o match:'@net' \
	    truss -t
}

atf_test_case unknown_group
unknown_group_head()
{
	atf_set descr "an unknown @group is rejected"
}
unknown_group_body()
{
	atf_check -s exit:1 -e match:'unknown system call group @nosuch' \
	    truss -t @nosuch /usr/bin/true
}

atf_test_case empty_term
empty_term_head()
{
	atf_set descr "an empty term adds nothing to the expression"
}
empty_term_body()
{
	require_truss
	printf 'hello\n' > input

	# An empty expression filters nothing, as if -t were absent.
	atf_check -s exit:0 -o inline:"hello\n" truss -o out -t '' cat input
	syscall_names out names
	some_name '^read$' names
	some_name '^openat$' names

	# A stray comma is ignored rather than being an error.
	atf_check -s exit:0 -o inline:"hello\n" \
	    truss -o out2 -t ',read,,write,' cat input
	syscall_names out2 names2
	only_names '^(read|write)$' names2
	some_name '^read$' names2

	# A negation with nothing to negate is still a mistake.
	atf_check -s exit:1 -e match:"missing pattern after" \
	    truss -t '!' /usr/bin/true
}

atf_test_case none
none_head()
{
	atf_set descr "@none selects no system call"
}
none_body()
{
	require_truss
	printf 'hello\n' > input

	# @none's member list is the single negated member "!*", so this
	# also covers a group whose members exclude rather than include.
	atf_check -s exit:0 -o inline:"hello\n" truss -o out -t @none cat input
	syscall_names out names
	atf_check -o empty cat names

	atf_check -s exit:0 -o inline:"hello\n" \
	    truss -o out2 -t '!@none' cat input
	syscall_names out2 names2
	some_name '^read$' names2
	some_name '^openat$' names2

	# @none is the empty set rather than a switch: it selects nothing
	# and leaves the terms before it alone.
	atf_check -s exit:0 -o inline:"hello\n" \
	    truss -o out3 -t '@file,@none' cat input
	syscall_names out3 names3
	some_name '^openat$' names3
}

atf_test_case by_number
by_number_head()
{
	atf_set descr "a term may name a system call by number"
}
by_number_body()
{
	require_truss
	printf 'hello\n' > input

	# 3 and 4 have been read(2) and write(2) since 4.2BSD.
	atf_check -s exit:0 -o inline:"hello\n" -e empty \
	    truss -o out -t 3 cat input
	syscall_names out names
	only_names '^read$' names
	some_name '^read$' names

	atf_check -s exit:0 -o inline:"hello\n" -e empty \
	    truss -o out2 -t 3,4 cat input
	syscall_names out2 names2
	only_names '^(read|write)$' names2

	# Numeric terms negate like any other.
	atf_check -s exit:0 -o inline:"hello\n" -e empty \
	    truss -o out3 -t '!3' cat input
	syscall_names out3 names3
	no_name '^read$' names3
	some_name '.' names3

	# Leading zeroes are still just a number.
	atf_check -s exit:0 -o inline:"hello\n" -e empty \
	    truss -o out4 -t 003 cat input
	syscall_names out4 names4
	only_names '^read$' names4
}

atf_test_case bad_number
bad_number_head()
{
	atf_set descr "only an unrepresentable system call number is rejected"
}
bad_number_body()
{
	require_truss
	printf 'hello\n' > input

	# A process may issue any number the kernel can hold, so a number
	# beyond the tables truss knows is not second-guessed.  It simply
	# does not match anything this program happens to call.
	atf_check -s exit:0 -o inline:"hello\n" -e empty \
	    truss -o out -t 99999 cat input
	syscall_names out names
	atf_check -o empty cat names

	# Too large to be a system call number at all: an error.
	atf_check -s exit:1 -e match:'system call number is too large' \
	    truss -t 4294967296 /usr/bin/true
}

atf_test_case unknown_syscall
unknown_syscall_head()
{
	atf_set descr "a term matching no system call warns but still runs"
}
unknown_syscall_body()
{
	require_truss
	printf 'hello\n' > input

	# A typo is a warning, not an error: the command still runs.
	atf_check -s exit:0 -o inline:"hello\n" \
	    -e match:'opne: matches no known system call' \
	    truss -o out -t opne cat input
	syscall_names out names
	atf_check -o empty cat names

	# So is a pattern that can never match.
	atf_check -s exit:0 -o inline:"hello\n" \
	    -e match:'matches no known system call' \
	    truss -o out2 -t 'raed*' cat input

	# Names of every ABI truss knows are accepted without complaint,
	# whether or not that ABI is the one being traced here.
	for name in read openat linux_write linux_newstat compat11.stat; do
		atf_check -s exit:0 -o inline:"hello\n" -e empty \
		    truss -o out3 -t "$name" cat input
	done

	# A number is the way to name a system call by number; '#' is not
	# a prefix truss accepts, so it is diagnosed like any other typo.
	atf_check -s exit:0 -o inline:"hello\n" \
	    -e match:'matches no known system call' \
	    truss -o out4 -t '#237' cat input
}

atf_test_case by_name
by_name_head()
{
	atf_set descr "a term naming one system call selects only that one"
}
by_name_body()
{
	require_truss
	printf 'hello\n' > input

	atf_check -s exit:0 -o inline:"hello\n" truss -o out -t read cat input
	syscall_names out names
	only_names '^read$' names
	some_name '^read$' names
}

atf_test_case by_pattern
by_pattern_head()
{
	atf_set descr "a term may be an fnmatch(3) pattern"
}
by_pattern_body()
{
	require_truss
	atf_check ln -s target link

	atf_check -s exit:0 -o ignore truss -o out -t 'readlink*' readlink link
	syscall_names out names
	only_names '^readlink' names
	some_name '^readlink' names
}

atf_test_case group
group_head()
{
	atf_set descr "an @group selects the system calls it names"
}
group_body()
{
	require_truss
	printf 'hello\n' > input

	atf_check -s exit:0 -o inline:"hello\n" truss -o out -t @read cat input
	syscall_names out names
	some_name '^read$' names
	no_name '^(openat|close|mmap|munmap|mprotect)$' names
}

atf_test_case group_read_excludes_readlink
group_read_excludes_readlink_head()
{
	atf_set descr "@read selects I/O reads but not readlink(2)"
}
group_read_excludes_readlink_body()
{
	require_truss
	atf_check ln -s target link

	# readlink(1) calls readlink(2), which @read must not select even
	# though "read*" does.
	atf_check -s exit:0 -o ignore truss -o out -t @read readlink link
	syscall_names out names
	no_name '^readlink' names
	some_name '^read$' names

	atf_check -s exit:0 -o ignore truss -o out2 -t 'read*' readlink link
	syscall_names out2 names2
	some_name '^readlink' names2
}

atf_test_case group_reference
group_reference_head()
{
	atf_set descr "@desc includes the members of @read and @write"
}
group_reference_body()
{
	require_truss
	printf 'hello\n' > input

	atf_check -s exit:0 -o inline:"hello\n" truss -o out -t @desc cat input
	syscall_names out names
	some_name '^read$' names
	some_name '^close$' names
}

atf_test_case negation
negation_head()
{
	atf_set descr "a term prefixed with ! excludes what it matches"
}
negation_body()
{
	require_truss

	atf_check -s exit:0 -o ignore truss -o out -t '!@all' /usr/bin/true
	syscall_names out names
	atf_check -o empty cat names

	atf_check -s exit:0 -o ignore truss -o out2 -t '!@memory' /usr/bin/true
	syscall_names out2 names2
	no_name '^(mmap|munmap|mprotect)$' names2
	some_name '.' names2
}

atf_test_case order
order_head()
{
	atf_set descr "the last term to match a system call wins"
}
order_body()
{
	require_truss
	printf 'hello\n' > input

	atf_check -s exit:0 -o ignore truss -o out -t 'read,!read' cat input
	syscall_names out names
	atf_check -o empty cat names

	atf_check -s exit:0 -o ignore truss -o out2 -t '!read,read' cat input
	syscall_names out2 names2
	only_names '^read$' names2
	some_name '^read$' names2
}

atf_test_case accumulate
accumulate_head()
{
	atf_set descr "repeating -t appends to the expression"
}
accumulate_body()
{
	require_truss
	atf_check ln -s target link

	atf_check -s exit:0 -o ignore \
	    truss -o out -t read -t readlink readlink link
	syscall_names out names
	only_names '^(read|readlink)$' names
	some_name '^read$' names
	some_name '^readlink$' names
}

atf_test_case count
count_head()
{
	atf_set descr "-c counts only the selected system calls"
}
count_body()
{
	require_truss
	printf 'hello\n' > input

	atf_check -s exit:0 -o inline:"hello\n" \
	    truss -c -o out -t read cat input
	atf_check -o match:'^read ' cat out
	atf_check -s exit:1 -o empty grep -q '^openat' out
}

atf_init_test_cases()
{
	atf_add_test_case list
	atf_add_test_case unknown_group
	atf_add_test_case empty_term
	atf_add_test_case none
	atf_add_test_case by_number
	atf_add_test_case bad_number
	atf_add_test_case unknown_syscall
	atf_add_test_case by_name
	atf_add_test_case by_pattern
	atf_add_test_case group
	atf_add_test_case group_read_excludes_readlink
	atf_add_test_case group_reference
	atf_add_test_case negation
	atf_add_test_case order
	atf_add_test_case accumulate
	atf_add_test_case count
}
