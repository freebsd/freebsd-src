#
# Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

setup()
{
	local md

	atf_check -o save:md mdconfig -a -t swap -s 1g
	md=/dev/$(cat md)
	atf_check -o ignore -e ignore newfs -t $@ $md

	cat <<__EOF__ > pjdfstest.toml
[features]
chflags = {}
posix_fallocate = {}
rename_ctime = {}
stat_st_birthtime = {}
utimensat = {}
utime_now = {}

[settings]
naptime = 0.001
__EOF__

	atf_check mkdir mnt
	atf_check mount $md ./mnt
}

doit()
{
	atf_check -o ignore pjdfstest -c pjdfstest.toml -p mnt
	atf_check umount ./mnt
	atf_check rmdir ./mnt
}

cleanup()
{
	if [ -d ./mnt ]; then
		umount ./mnt
	fi
	if [ -s md ]; then
		mdconfig -d -u $(cat md)
	fi
}

atf_test_case ufs1 cleanup
ufs1_head()
{
	atf_set descr "Run pjdfstest on a UFS1 filesystem"
	atf_set require.user root
	atf_set require.progs pjdfstest
}
ufs1_body()
{
	setup -O 1
	# UFS1 doesn't have 64-bit timestamps or a birthtime field in the inode.
	cat >> pjdfstest.toml <<__EOF__
expected_failures = [
"utimensat::birthtime",
"utimensat::y2038"
]
__EOF__
	doit
}
ufs1_cleanup()
{
	cleanup
}

atf_test_case ufs2_nosu cleanup
ufs2_nosu_head()
{
	atf_set descr "Run pjdfstest on a UFS2 filesystem without soft updates"
	atf_set require.user root
	atf_set require.progs pjdfstest
}
ufs2_nosu_body()
{
	setup -O 2 -u
	doit
}
ufs2_nosu_cleanup()
{
	cleanup
}

atf_test_case ufs2_su cleanup
ufs2_su_head()
{
	atf_set descr "Run pjdfstest on a UFS2 filesystem with soft updates"
	atf_set require.user root
	atf_set require.progs pjdfstest
}
ufs2_su_body()
{
	setup -O 2 -U
	doit
}
ufs2_su_cleanup()
{
	cleanup
}

atf_test_case ufs2_suj cleanup
ufs2_suj_head()
{
	atf_set descr "Run pjdfstest on a UFS2 filesystem with soft updates journaling"
	atf_set require.user root
	atf_set require.progs pjdfstest
}
ufs2_suj_body()
{
	setup -O 2 -U -j
	doit
}
ufs2_suj_cleanup()
{
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case ufs1
	atf_add_test_case ufs2_nosu
	atf_add_test_case ufs2_su
	atf_add_test_case ufs2_suj
}
