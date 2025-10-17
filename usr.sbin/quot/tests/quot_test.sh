#
# Copyright (c) 2025 Dag-Erling Smørgrav <des@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

# Create and mount a UFS filesystem on a small memory disk
quot_setup()
{
	atf_check -o save:dev mdconfig -t malloc -s 16M
	local dev=$(cat dev)
	atf_check -o ignore newfs "$@" /dev/$dev
	atf_check mkdir mnt
	local mnt=$(realpath mnt)
	atf_check mount /dev/$dev "$mnt"
	echo "/dev/$dev: ($mnt)" >expect
	printf "%5d\t%5d\t%-8s\n" 8 2 "#0" >>expect
}

# Create a directory owned by a given UID
quot_adduid()
{
	local uid=$1
	atf_check install -d -o $uid -g 0 mnt/$uid
	printf "%5d\t%5d\t%-8s\n" 4 1 "#$uid" >>expect
}

# Perform the tests
quot_test()
{
	local dev=$(cat dev)
	# Create inodes owned by a large number of users to exercise
	# hash collisions and rehashing.  The code uses an open hash
	# table that starts out with only 8 entries and doubles every
	# time it fills up.
	local uid
	for uid in $(seq 1 32); do
		quot_adduid $uid
	done
	# Also create inodes owned by users with long UIDs, up to the
	# highest possible value (2^32 - 2, because chown(2) and
	# friends interpret 2^32 - 1 as “leave unchanged”).
	local shift
	for shift in $(seq 6 32); do
		quot_adduid $(((1 << shift) - 2))
	done
	# Since quot operates directly on the underlying device, not
	# on the mounted filesystem, we remount read-only to ensure
	# that everything gets flushed to the memory disk.
	atf_check mount -ur /dev/$dev
	atf_check -o file:expect quot -fkN /dev/$dev
	atf_check -o file:expect quot -fkN $(realpath mnt)
}

# Unmount and release the memory disk
quot_cleanup()
{
	if [ -d mnt ]; then
		umount mnt || true
	fi
	if [ -f dev ]; then
		mdconfig -d -u $(cat dev) || true
	fi
}

atf_test_case ufs1 cleanup
ufs1_head()
{
	atf_set descr "Test quot on UFS1"
	atf_set require.user root
}
ufs1_body()
{
	quot_setup -O1
	quot_test
}
ufs1_cleanup()
{
	quot_cleanup
}

atf_test_case ufs2 cleanup
ufs2_head()
{
	atf_set descr "Test quot on UFS2"
	atf_set require.user root
}
ufs2_body()
{
	quot_setup -O2
	quot_test
}
ufs2_cleanup()
{
	quot_cleanup
}

atf_init_test_cases()
{
	atf_add_test_case ufs1
	atf_add_test_case ufs2
}
