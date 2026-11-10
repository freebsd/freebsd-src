#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2026 Moin Rahman <bofh@FreeBSD.org>
#

# Write a master.passwd containing root foloowed by the given entry.
mkpasswd()
{
	printf '%s\n' 'root:*:0:0::0:0:Charlie &:/root:/bin/sh' "$1" \
	    > master.passwd
	chmod 600 master.passwd
}

# Expect pwd_mkdb to reject the given login name and leave no databases.
reject()
{
	mkpasswd "$1:*:1001:1001::0:0:test:/nonexistent:/usr/sbin/nologin"
	atf_check -s exit:1 -e match:"invalid login name at line #2" \
	    pwd_mkdb -p -d "$PWD" "$PWD/master.passwd"
	atf_check test ! -e pwd.db
	atf_check test ! -e spwd.db
	atf_check test ! -e passwd
}

# Expect pwd_mkdb to accept the given master.passwd entry.
accept()
{
	mkpasswd "$1"
	atf_check pwd_mkdb -p -d "$PWD" "$PWD/master.passwd"
	atf_check test -e pwd.db
	atf_check test -e spwd.db
	rm -f passwd pwd.db spwd.db
}

atf_test_case reject_shell_metachars
reject_shell_metachars_head()
{
	atf_set "descr" "Login names with shell metacharacters are rejected"
}
reject_shell_metachars_body()
{
	reject 'user1$(id$IFS-A)'
	reject 'a;b'
	reject 'a|b'
	reject 'a`id`'
	reject "a'b"
	reject 'a&b'
	reject 'a>b'
	reject 'a"b'
	reject 'a\b'
	reject 'a/b'
	reject 'a b'
	reject 'ma$ch'
}

atf_test_case reject_malformed
reject_malformed_head()
{
	atf_set "descr" "Empty, hyphen-led, control and 8-bit login names are rejected"
}
reject_malformed_body()
{
	reject ''
	reject "$(printf 'esc\033[2J')"
	reject "$(printf 'tab\tname')"
	reject "$(printf 'caf\303\251')"
	reject "$(printf 'del\177')"
}

atf_test_case accept_valid
accept_valid_head()
{
	atf_set "descr" "Valid login names, including a trailing dollar, are accepted"
}
accept_valid_body()
{
	accept 'first.last_1-x:*:1001:1001::0:0:test:/nonexistent:/usr/sbin/nologin'
	accept 'mach$:*:1002:1002::0:0:test:/nonexistent:/usr/sbin/nologin'
	accept '_daemon:*:1003:1003::0:0:test:/nonexistent:/usr/sbin/nologin'
}

atf_test_case nis_compat
nis_compat_head()
{
	atf_set "descr" "NIS compat entries are accepted and their names checked"
}
nis_compat_body()
{
	accept '+:::::::::'
	accept '+user:::::::::'
	accept '-user:::::::::'
	accept '+@netgroup:::::::::'
	accept '-@netgroup:::::::::'
	reject '-'
	reject '+@'
	reject '+@ng$(id)'
	reject '-x$(id)'
	reject '+-x'
}

atf_init_test_cases()
{
	atf_add_test_case reject_shell_metachars
	atf_add_test_case reject_malformed
	atf_add_test_case accept_valid
	atf_add_test_case nis_compat
}
