#
# Copyright (c) 2016 Spectra Logic Corp
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#   
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$

atf_test_case bad_namespace
bad_namespace_head() {
	atf_set "descr" "Can't set attributes for nonexistent namespaces"
}
bad_namespace_body() {
	touch foo
	atf_check -s not-exit:0 -e match:"Invalid argument" \
		setextattr badnamespace myattr X foo
	atf_check -s not-exit:0 -e match:"Invalid argument" \
		lsextattr -q badnamespace foo
}	

atf_test_case hex
hex_head() {
	atf_set "descr" "Set and get attribute values in hexadecimal"
}
hex_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr user myattr1 XYZ foo
	atf_check -s exit:0 -o inline:"58 59 5a\n" \
		getextattr -qx user myattr1 foo
}	

atf_test_case long_name
long_name_head() {
	atf_set "descr" "A maximum length attribute name"
}
long_name_body() {
	# https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=208965
	atf_expect_fail "BUG 208965 extattr(2) doesn't allow maxlen attr names"

	ATTRNAME=`jot -b X -s "" 255 0`
	touch foo
	atf_check -s exit:0 -o empty setextattr user $ATTRNAME myvalue foo
	atf_check -s exit:0 -o inline:"${ATTRNAME}\n" lsextattr -q user foo
	atf_check -s exit:0 -o inline:"myvalue\n" \
		getextattr -q user ${ATTRNAME} foo
	atf_check -s exit:0 -o empty rmextattr user ${ATTRNAME} foo
	atf_check -s exit:0 -o empty lsextattr -q user foo
}	

atf_test_case noattrs
noattrs_head() {
	atf_set "descr" "A file with no extended attributes"
}
noattrs_body() {
	touch foo
	atf_check -s exit:0 -o empty lsextattr -q user foo
}	

atf_test_case nonexistent_file
nonexistent_file_head() {
	atf_set "descr" "A file that does not exist"
}
nonexistent_file_body() {
	atf_check -s exit:1 -e match:"No such file or directory" \
		lsextattr user foo
	atf_check -s exit:1 -e match:"No such file or directory" \
		setextattr user myattr myvalue foo
	atf_check -s exit:1 -e match:"No such file or directory" \
		getextattr user myattr foo
	atf_check -s exit:1 -e match:"No such file or directory" \
		rmextattr user myattr foo
}	

atf_test_case null
null_head() {
	atf_set "descr" "NUL-terminate an attribute value"
}
null_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr -n user myattr myvalue foo
	atf_check -s exit:0 -o inline:"myvalue\0\n" getextattr -q user myattr foo
}	

atf_test_case one_user_attr
one_user_attr_head() {
	atf_set "descr" "A file with one extended attribute"
}
one_user_attr_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr user myattr myvalue foo
	atf_check -s exit:0 -o inline:"myattr\n" lsextattr -q user foo
	atf_check -s exit:0 -o inline:"myvalue\n" getextattr -q user myattr foo
	atf_check -s exit:0 -o empty rmextattr user myattr foo
	atf_check -s exit:0 -o empty lsextattr -q user foo
}	

atf_test_case one_system_attr
one_system_attr_head() {
	atf_set "descr" "A file with one extended attribute"
	atf_set "require.user" "root"
}
one_system_attr_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr system myattr myvalue foo
	atf_check -s exit:0 -o inline:"myattr\n" lsextattr -q system foo
	atf_check -s exit:0 -o inline:"myvalue\n" getextattr -q system myattr foo
	atf_check -s exit:0 -o empty rmextattr system myattr foo
	atf_check -s exit:0 -o empty lsextattr -q system foo
}	

atf_test_case stringify
stringify_head() {
	atf_set "descr" "Stringify the output of getextattr"
}
stringify_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr user myattr "my value" foo
	atf_check -s exit:0 -o inline:"\"my\\\040value\"\n" \
		getextattr -qs user myattr foo
}	

atf_test_case symlink
symlink_head() {
	atf_set "descr" "A symlink to an ordinary file"
}
symlink_body() {
	touch foo
	ln -s foo foolink
	atf_check -s exit:0 -o empty setextattr user myattr myvalue foolink
	atf_check -s exit:0 -o inline:"myvalue\n" \
		getextattr -q user myattr foolink
	atf_check -s exit:0 -o inline:"myvalue\n" getextattr -q user myattr foo
}

atf_test_case symlink_nofollow
symlink_nofollow_head() {
	atf_set "descr" "Operating directly on a symlink"
}
symlink_nofollow_body() {
	touch foo
	ln -s foo foolink
	# Check that with -h we can operate directly on the link
	atf_check -s exit:0 -o empty setextattr -h user myattr myvalue foolink
	atf_check -s exit:0 -o inline:"myvalue\n" \
		getextattr -qh user myattr foolink
	atf_check -s exit:1 -e match:"Attribute not found" \
		getextattr user myattr foolink
	atf_check -s exit:1 -e match:"Attribute not found" \
		getextattr user myattr foo

	# Check that with -h we cannot operate on the destination file
	atf_check -s exit:0 -o empty setextattr user otherattr othervalue foo
	atf_check -s exit:1 getextattr -qh user otherattr foolink
}

atf_test_case system_and_user_attrs
system_and_user_attrs_head() {
	atf_set "descr" "A file with both system and user extended attributes"
	atf_set "require.user" "root"
}
system_and_user_attrs_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr user userattr userval foo
	atf_check -s exit:0 -o empty setextattr system sysattr sysval foo
	atf_check -s exit:0 -o inline:"userattr\n" lsextattr -q user foo
	atf_check -s exit:0 -o inline:"sysattr\n" lsextattr -q system foo

	atf_check -s exit:0 -o inline:"userval\n" getextattr -q user userattr foo
	atf_check -s exit:0 -o inline:"sysval\n" getextattr -q system sysattr foo
	atf_check -s exit:0 -o empty rmextattr user userattr foo
	atf_check -s exit:0 -o empty rmextattr system sysattr foo
	atf_check -s exit:0 -o empty lsextattr -q user foo
	atf_check -s exit:0 -o empty lsextattr -q system foo
}	

atf_test_case two_files
two_files_head() {
	atf_set "descr" "Manipulate two files"
}
two_files_body() {
	touch foo bar
	atf_check -s exit:0 -o empty setextattr user myattr myvalue foo bar
	atf_check -s exit:0 -o inline:"foo\tmyattr\nbar\tmyattr\n" \
		lsextattr user foo bar
	atf_check -s exit:0 \
		-o inline:"foo\tmyvalue\nbar\tmyvalue\n" \
		getextattr user myattr foo bar
	atf_check -s exit:0 -o empty rmextattr user myattr foo bar
	atf_check -s exit:0 -o empty lsextattr -q user foo bar
}

atf_test_case two_files_force
two_files_force_head() {
	atf_set "descr" "Manipulate two files.  The first does not exist"
}
two_files_force_body() {
	touch bar
	atf_check -s exit:1 -e match:"No such file or directory" \
		setextattr user myattr myvalue foo bar
	atf_check -s exit:0 -e ignore setextattr -f user myattr myvalue foo bar
	atf_check -s exit:1 -e match:"No such file or directory" \
		lsextattr user foo bar
	atf_check -s exit:0 -e ignore -o inline:"bar\tmyattr\n" \
		lsextattr -f user foo bar
	atf_check -s exit:1 -e match:"No such file or directory" \
		getextattr user myattr foo bar
	atf_check -s exit:0 -e ignore \
		-o inline:"bar\tmyvalue\n" \
		getextattr -f user myattr foo bar
	atf_check -s exit:1 -e match:"No such file or directory" \
		rmextattr user myattr foo bar
	atf_check -s exit:0 -e ignore \
		rmextattr -f user myattr foo bar
	atf_check -s exit:0 -o empty lsextattr -q user bar
}

atf_test_case two_user_attrs
two_user_attrs_head() {
	atf_set "descr" "A file with two extended attributes"
}
two_user_attrs_body() {
	touch foo
	atf_check -s exit:0 -o empty setextattr user myattr1 myvalue1 foo
	atf_check -s exit:0 -o empty setextattr user myattr2 myvalue2 foo
	# lsextattr could return the attributes in any order, so we must be
	# careful how we compare them.
	raw_output=`lsextattr -q user foo` || atf_fail "lsextattr failed"
	tabless_output=`printf "%s %s" ${raw_output}`
	if [ "myattr1 myattr2" != "${tabless_output}" -a \
	     "myattr2 myattr1" != "${tabless_output}" ]; then
		atf_fail "lsextattr printed ${tabless_output}"
	fi
	atf_check -s exit:0 -o inline:"myvalue1\n" getextattr -q user myattr1 foo
	atf_check -s exit:0 -o inline:"myvalue2\n" getextattr -q user myattr2 foo
	atf_check -s exit:0 -o empty rmextattr user myattr2 foo
	atf_check -s exit:0 -o empty rmextattr user myattr1 foo
	atf_check -s exit:0 -o empty lsextattr -q user foo
}	

atf_test_case unprivileged_user_cannot_set_system_attr
unprivileged_user_cannot_set_system_attr_head() {
	atf_set "descr" "Unprivileged users can't set system attributes"
        atf_set "require.user" "unprivileged"
}
unprivileged_user_cannot_set_system_attr_body() {
	touch foo
	atf_check -s exit:1 -e match:"Operation not permitted" \
		setextattr system myattr myvalue foo
}	


atf_init_test_cases() {
	# TODO: add test cases for verbose output (without -q)
	atf_add_test_case bad_namespace
	atf_add_test_case hex
	atf_add_test_case long_name
	atf_add_test_case noattrs
	atf_add_test_case nonexistent_file
	atf_add_test_case null
	atf_add_test_case symlink_nofollow
	atf_add_test_case one_user_attr
	atf_add_test_case one_system_attr
	atf_add_test_case stringify
	atf_add_test_case symlink
	atf_add_test_case symlink_nofollow
	atf_add_test_case system_and_user_attrs
	atf_add_test_case two_files
	atf_add_test_case two_files_force
	atf_add_test_case two_user_attrs
	atf_add_test_case unprivileged_user_cannot_set_system_attr
}
