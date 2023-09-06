#
# Copyright (c) 2023 Klara, Inc.
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case hash_comment
hash_comment_head() {
	atf_set descr "multiline comment follows directive"
}
hash_comment_body() {
	cat >f <<EOF
#if FOO
#endif /*
*/
EOF
	atf_check -o file:f unifdef <f
}

atf_test_case redefine
redefine_head() {
	atf_set descr "redefine the same symbol"
}
redefine_body() {
	cat >file <<EOF
#if FOO
a
#else
b
#endif
EOF
	atf_check -s exit:1 -o inline:"a\n" unifdef -DFOO <file
	atf_check -s exit:1 -o inline:"a\n" unifdef -UFOO -DFOO <file
	atf_check -s exit:1 -o inline:"a\n" unifdef -DFOO=0 -DFOO <file
	atf_check -s exit:1 -o inline:"b\n" unifdef -UFOO <file
	atf_check -s exit:1 -o inline:"b\n" unifdef -DFOO -UFOO <file
	atf_check -s exit:1 -o inline:"b\n" unifdef -DFOO -DFOO=0 <file
}

atf_init_test_cases() {
	atf_add_test_case hash_comment
	atf_add_test_case redefine
}
