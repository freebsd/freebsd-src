# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2014 Spectra Logic.  All rights reserved.
# Use is subject to license terms.
#


atf_test_case umount_001 cleanup
umount_001_head()
{
	atf_set "descr" "zfs umount should unmount a file system"
	atf_set "require.progs" "ksh93 zfs"
}
umount_001_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/vars.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/mounttest.ksh -u umount || \
		atf_fail "Testcase failed"
}
umount_001_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/vars.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}

atf_test_case umountall_001 cleanup
umountall_001_head()
{
	atf_set "descr" "zfs umount -a should unmount all ZFS file systems"
	atf_set "require.progs" "ksh93 zfs"
}
umountall_001_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/vars.cfg

	if other_pools_exist; then
		atf_skip "Can't test unmount -a with existing pools"
	fi

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/mounttest.ksh -u 'umount -a' || \
		atf_fail "Testcase failed"
}
umountall_001_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/vars.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}

atf_init_test_cases()
{

	atf_add_test_case umount_001
	atf_add_test_case umountall_001
}
