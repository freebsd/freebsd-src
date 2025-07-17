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


atf_test_case zfs_unallow_001_pos cleanup
zfs_unallow_001_pos_head()
{
	atf_set "descr" "Verify '-l' only removed the local permissions."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_001_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_001_pos.ksh || atf_fail "Testcase failed"
}
zfs_unallow_001_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_002_pos cleanup
zfs_unallow_002_pos_head()
{
	atf_set "descr" "Verify '-d' only removed the descendent permissions."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_002_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_002_pos.ksh || atf_fail "Testcase failed"
}
zfs_unallow_002_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_003_pos cleanup
zfs_unallow_003_pos_head()
{
	atf_set "descr" "Verify options '-r' and '-l'+'-d' will unallow permission tothis dataset and the descendent datasets."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_003_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_003_pos.ksh || atf_fail "Testcase failed"
}
zfs_unallow_003_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_004_pos cleanup
zfs_unallow_004_pos_head()
{
	atf_set "descr" "Verify '-s' will remove permissions from the named set."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_004_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_004_pos.ksh || atf_fail "Testcase failed"
}
zfs_unallow_004_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_005_pos cleanup
zfs_unallow_005_pos_head()
{
	atf_set "descr" "Verify option '-c' will remove the created permission set."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_005_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_005_pos.ksh || atf_fail "Testcase failed"
}
zfs_unallow_005_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_006_pos cleanup
zfs_unallow_006_pos_head()
{
	atf_set "descr" "Verify option '-u', '-g' and '-e' only removed the specified typepermissions set."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_006_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_006_pos.ksh || atf_fail "Testcase failed"
}
zfs_unallow_006_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_007_neg cleanup
zfs_unallow_007_neg_head()
{
	atf_set "descr" "zfs unallow won't remove those permissions which inherited fromits parent dataset."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_007_neg_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_007_neg.ksh || atf_fail "Testcase failed"
}
zfs_unallow_007_neg_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zfs_unallow_008_neg cleanup
zfs_unallow_008_neg_head()
{
	atf_set "descr" "zfs unallow can handle invalid arguments."
	atf_set "require.progs" "ksh93 zfs sudo"
}
zfs_unallow_008_neg_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zfs_unallow_008_neg.ksh || atf_fail "Testcase failed"
}
zfs_unallow_008_neg_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
    . $(atf_get_srcdir)/delegate_common.kshlib
    . $(atf_get_srcdir)/delegate.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case zfs_unallow_001_pos
	atf_add_test_case zfs_unallow_002_pos
	atf_add_test_case zfs_unallow_003_pos
	atf_add_test_case zfs_unallow_004_pos
	atf_add_test_case zfs_unallow_005_pos
	atf_add_test_case zfs_unallow_006_pos
	atf_add_test_case zfs_unallow_007_neg
	atf_add_test_case zfs_unallow_008_neg
}
