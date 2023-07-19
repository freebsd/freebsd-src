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

atf_init_test_cases() {
	atf_add_test_case hash_comment
}
