#
# Copyright (c) 2024 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

magic_words="Squeamish $$ Ossifrage"

atf_test_case basic
basic_head()
{
	atf_set "descr" "Basic test case"
}
basic_body()
{
	atf_check -o match:"^magic_words=${magic_words}\$" \
		  env magic_words="${magic_words}"
	export MAGIC_WORDS="${magic_words}"
	atf_check -o match:"^MAGIC_WORDS=${magic_words}\$" \
		  env
	unset MAGIC_WORDS
}

atf_test_case unset
unset_head()
{
	atf_set "descr" "Unset a variable"
}
unset_body()
{
	export MAGIC_WORDS="${magic_words}"
	atf_check -o not-match:"^MAGIC_WORDS=" \
		  env -u MAGIC_WORDS
	unset MAGIC_WORDS
}

atf_test_case empty
empty_head()
{
	atf_set "descr" "Empty environment"
}
empty_body()
{
	atf_check env -i
}

atf_test_case true
true_head()
{
	atf_set "descr" "Run true"
}
true_body()
{
	atf_check env true
}

atf_test_case false
false_head()
{
	atf_set "descr" "Run false"
}
false_body()
{
	atf_check -s exit:1 env false
}

atf_test_case false
false_head()
{
	atf_set "descr" "Run false"
}
false_body()
{
	atf_check -s exit:1 env false
}

atf_test_case altpath
altpath_head()
{
	atf_set "descr" "Use alternate path"
}
altpath_body()
{
	echo "echo ${magic_words}" >magic_words
	chmod 0755 magic_words
	atf_check -s exit:125 -e match:"must specify command" \
		  env -P "${PWD}"
	atf_check -s exit:127 -e match:"No such file" \
		  env magic_words
	atf_check -o inline:"${magic_words}\n" \
		  env -P "${PWD}" magic_words
}

atf_test_case equal
equal_head()
{
	atf_set "descr" "Command name contains equal sign"
}
equal_body()
{
	echo "echo ${magic_words}" >"magic=words"
	chmod 0755 "magic=words"
	atf_check -o match:"^${PWD}/magic=words$" \
		  env "${PWD}/magic=words"
	atf_check -s exit:125 -e match:"must specify command" \
		  env -P "${PATH}:${PWD}" "magic=words"
	atf_check -o inline:"${magic_words}\n" \
		  env command "${PWD}/magic=words"
	atf_check -o inline:"${magic_words}\n" \
		  env PATH="${PATH}:${PWD}" command "magic=words"
}

atf_test_case chdir
chdir_head()
{
	atf_set "descr" "Change working directory"
}
chdir_body()
{
	local subdir="dir.$$"
	atf_check -o inline:"${PWD}\n" \
		  env pwd
	atf_check -s exit:125 -e match:"must specify command" \
		  env -C "${subdir}"
	atf_check -s exit:125 \
		  -e match:"cannot change directory to '${subdir}':" \
		  env -C "${subdir}" pwd
	atf_check mkdir "${subdir}"
	atf_check -o inline:"${PWD}/${subdir}\n" \
		  env -C "${subdir}" pwd
}

atf_test_case stdout
stdout_head()
{
	atf_set descr "Failure to write to stdout"
}
stdout_body()
{
	(
		trap "" PIPE
		sleep 1
		env 2>stderr
		echo $? >result
	) | true
	atf_check -o inline:"1\n" cat result
	atf_check -o match:"stdout" cat stderr
}

atf_init_test_cases()
{
	atf_add_test_case basic
	atf_add_test_case unset
	atf_add_test_case empty
	atf_add_test_case true
	atf_add_test_case false
	atf_add_test_case altpath
	atf_add_test_case equal
	atf_add_test_case chdir
	atf_add_test_case stdout
}
