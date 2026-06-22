#
# Copyright (c) 2026 Mark Johnston <markj@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

setup()
{
	local disk pool

	truncate -s 1g disk

	cat <<__EOF__ > pjdfstest.toml
[features]
chflags = {}
rename_ctime = {}
stat_st_birthtime = {}
utimensat = {}
utime_now = {}

[settings]
naptime = 0.001
__EOF__

	atf_check mkdir mnt
	pool=pjdfstest$$
	atf_check zpool create -m $(pwd)/mnt $pool $(pwd)/disk
	echo $pool > pool
}

doit()
{
	local mp

	mp=${1:-mnt}
	atf_check -o ignore pjdfstest -c pjdfstest.toml -p $mp
	if [ "$mp" != "mnt" ]; then
		atf_check umount $mp
		atf_check rmdir $mp
	fi
	atf_check zpool destroy $(cat pool)
	atf_check rm -f pool
}

cleanup()
{
	if [ -s pool ]; then
		zpool destroy $(cat pool)
	fi
}

atf_test_case zfs cleanup
zfs_head()
{
	atf_set descr "Checks that pjdfstest passes on zfs"
	atf_set require.user root
	atf_set require.progs pjdfstest
}
zfs_body()
{
	setup
	doit
}
zfs_cleanup()
{
	cleanup
}

atf_test_case zfs_nullfs cleanup
zfs_nullfs_head()
{
	atf_set descr "Checks that pjdfstest passes on zfs mounted via nullfs"
	atf_set require.user root
	atf_set require.progs pjdfstest
}
zfs_nullfs_body()
{
	setup
	atf_check mkdir mnt2
	atf_check mount -t nullfs mnt mnt2
	doit mnt2
}
zfs_nullfs_cleanup()
{
	if [ -d mnt2 ]; then
		umount mnt2
		rmdir mnt2
	fi
	cleanup
}

atf_init_test_cases()
{
	atf_add_test_case zfs
	atf_add_test_case zfs_nullfs
}
