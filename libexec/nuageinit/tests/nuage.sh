#-
# Copyright (c) 2022 Baptiste Daroussin <bapt@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

export NUAGE_FAKE_ROOTDIR="$PWD"

atf_test_case sethostname
atf_test_case addsshkey
atf_test_case adduser
atf_test_case addgroup

sethostname_body()
{
	atf_check /usr/libexec/flua $(atf_get_srcdir)/sethostname.lua
	if [ ! -f etc/rc.conf.d/hostname ]; then
		atf_fail "hostname not written"
	fi
	atf_check -o inline:"hostname=\"myhostname\"\n" cat etc/rc.conf.d/hostname
}

addsshkey_body()
{
	atf_check /usr/libexec/flua $(atf_get_srcdir)/addsshkey.lua
	if [ ! -f .ssh/authorized_keys ]; then
		atf_fail "ssh key not added"
	fi
	atf_check -o inline:"40700\n" stat -f %p .ssh
	atf_check -o inline:"100600\n" stat -f %p .ssh/authorized_keys
	atf_check -o inline:"mykey\n" cat .ssh/authorized_keys
	atf_check /usr/libexec/flua $(atf_get_srcdir)/addsshkey.lua
	atf_check -o inline:"mykey\nmykey\n" cat .ssh/authorized_keys
}

adduser_head()
{
	atf_set "require.user" root
}
adduser_body()
{
	mkdir etc
	printf "root:*:0:0::0:0:Charlie &:/root:/bin/sh\n" > etc/master.passwd
	pwd_mkdb -d etc etc/master.passwd
	printf "wheel:*:0:root\n" > etc/group
	atf_check -e inline:"nuageinit: Argument should be a table\nnuageinit: Argument should be a table\n" /usr/libexec/flua $(atf_get_srcdir)/adduser.lua
	test -d home/impossible_username || atf_fail "home not created"
	atf_check -o inline:"impossible_username::1001:1001::0:0:impossible_username User:/home/impossible_username:/bin/sh\n" grep impossible_username etc/master.passwd
}

addgroup_body()
{
	mkdir etc
	printf "wheel:*:0:root\n" > etc/group
	atf_check -e inline:"nuageinit: Argument should be a table\nnuageinit: Argument should be a table\n" /usr/libexec/flua $(atf_get_srcdir)/addgroup.lua
	atf_check -o inline:"impossible_groupname:*:1001:\n" grep impossible_groupname etc/group
}

atf_init_test_cases()
{
	atf_add_test_case sethostname
	atf_add_test_case addsshkey
	atf_add_test_case adduser
	atf_add_test_case addgroup
}
