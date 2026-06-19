#
# Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

setup()
{
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
	atf_check mount -t tmpfs none mnt
}

doit()
{
	atf_check -o ignore pjdfstest -c pjdfstest.toml -p mnt
	atf_check umount mnt
	atf_check rmdir mnt
}

cleanup()
{
	if [ -d ./mnt ]; then
		umount ./mnt
	fi
}

atf_test_case tmpfs cleanup
tmpfs_head()
{
	atf_set descr "Checks that pjdfstest passes on tmpfs"
	atf_set require.progs pjdfstest
	atf_set require.user root
}
tmpfs_body()
{
	setup
	doit
}
tmpfs_cleanup()
{
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case tmpfs
}
