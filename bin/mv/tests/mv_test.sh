#!/usr/bin/env atf-sh
#-
# Copyright (c) 2024 Stefan Eßer <se@FreeBSD.org>
#
# SPDX-License-Identifier: BSD-2-Clause
#

atf_test_case wildcard_msdosfs cleanup
wildcard_msdosfs_head()
{
	atf_set "descr" "Verify that moving to * prints out the right error on MS-DOS file systems"
	atf_set "require.user" root
}
wildcard_msdosfs_body()
{
	# Create an MS-DOS FS mount
	md=$(mdconfig -a -t swap -s 5m)
	mkdir mnt
	newfs_msdos -h 1 -u 63 "$md"
	mount_msdosfs /dev/"${md}" mnt

	atf_check -e empty -o empty -s exit:0 touch mnt/A.DAT
	atf_check -e match:'No such file or directory' -o empty -s exit:1 \
	    mv mnt/A.DAT mnt/B*.DAT
}
wildcard_msdosfs_cleanup()
{
	umount mnt
	mdconfig -d -u /dev/"${md}"
}

atf_init_test_cases()
{
	atf_add_test_case wildcard_msdosfs
}
