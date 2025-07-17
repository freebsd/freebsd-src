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
# Copyright 2012 Spectra Logic.  All rights reserved.
# Use is subject to license terms.
#


atf_test_case zfs_copies_001_pos cleanup
zfs_copies_001_pos_head()
{
	atf_set "descr" "Verify 'copies' property with correct arguments works or not."
	atf_set "require.progs" "ksh93 zfs"
}
zfs_copies_001_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_copies_001_pos.ksh || atf_fail "Testcase failed"
}
zfs_copies_001_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_copies_002_pos cleanup
zfs_copies_002_pos_head()
{
	atf_set "descr" "Verify that the space used by multiple copies is charged correctly."
	atf_set "require.progs" "ksh93 zfs"
}
zfs_copies_002_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_copies_002_pos.ksh || atf_fail "Testcase failed"
}
zfs_copies_002_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_copies_003_pos cleanup
zfs_copies_003_pos_head()
{
	atf_set "descr" "Verify that ZFS volume space used by multiple copies is charged correctly."
	atf_set "require.progs" "ksh93 zfs"
}
zfs_copies_003_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	verify_zvol_recursive
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_copies_003_pos.ksh || atf_fail "Testcase failed"
}
zfs_copies_003_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_copies_004_neg cleanup
zfs_copies_004_neg_head()
{
	atf_set "descr" "Verify that copies property cannot be set to any value other than 1,2 or 3"
	atf_set "require.progs" "ksh93 zfs"
}
zfs_copies_004_neg_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	verify_disk_count "$DISKS" 1
	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_copies_004_neg.ksh || atf_fail "Testcase failed"
}
zfs_copies_004_neg_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_copies_005_neg cleanup
zfs_copies_005_neg_head()
{
	atf_set "descr" "Verify that copies cannot be set with pool version 1"
	atf_set "require.progs" "ksh93 zfs zpool"
}
zfs_copies_005_neg_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_copies_005_neg.ksh || atf_fail "Testcase failed"
}
zfs_copies_005_neg_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_copies_006_pos cleanup
zfs_copies_006_pos_head()
{
	atf_set "descr" "Verify that ZFS volume space used by multiple copies is charged correctly."
	atf_set "require.progs" "ksh93 zfs"
}
zfs_copies_006_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_copies_006_pos.ksh || atf_fail "Testcase failed"
}
zfs_copies_006_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zfs_copies.kshlib
	. $(atf_get_srcdir)/zfs_copies.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case zfs_copies_001_pos
	atf_add_test_case zfs_copies_002_pos
	atf_add_test_case zfs_copies_003_pos
	atf_add_test_case zfs_copies_004_neg
	atf_add_test_case zfs_copies_005_neg
	atf_add_test_case zfs_copies_006_pos
}
