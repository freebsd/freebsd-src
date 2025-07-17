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


atf_test_case zpool_add_001_pos cleanup
zpool_add_001_pos_head()
{
	atf_set "descr" "'zpool add <pool> <vdev> ...' can add devices to the pool."
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_001_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 5
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_001_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_001_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_002_pos cleanup
zpool_add_002_pos_head()
{
	atf_set "descr" "'zpool add -f <pool> <vdev> ...' can successfully add devices to the pool in some cases."
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_002_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 3
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_002_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_002_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_003_pos cleanup
zpool_add_003_pos_head()
{
	atf_set "descr" "'zpool add -n <pool> <vdev> ...' can display the configuration without actually adding devices to the pool."
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_003_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 2
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_003_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_003_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_004_pos cleanup
zpool_add_004_pos_head()
{
	atf_set "descr" "'zpool add <pool> <vdev> ...' can add zfs volume to the pool."
	atf_set "require.progs" "ksh93 zfs zpool"
	atf_set "timeout" 2400
}
zpool_add_004_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 2
	verify_zvol_recursive
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_004_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_004_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_005_pos cleanup
zpool_add_005_pos_head()
{
	atf_set "descr" "'zpool add' should fail with inapplicable scenarios."
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_005_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 5
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_005_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_005_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_006_pos cleanup
zpool_add_006_pos_head()
{
	atf_set "descr" "'zpool add [-f]' can add large numbers of vdevs to the specified pool without any errors."
	atf_set "require.progs" "ksh93 zfs zpool"
	atf_set "timeout" 2400
}
zpool_add_006_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_006_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_006_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_007_neg cleanup
zpool_add_007_neg_head()
{
	atf_set "descr" "'zpool add' should return an error with badly-formed parameters."
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_007_neg_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 2
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_007_neg.ksh || atf_fail "Testcase failed"
}
zpool_add_007_neg_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_008_neg cleanup
zpool_add_008_neg_head()
{
	atf_set "descr" "'zpool add' should return an error with nonexistent pools and vdevs"
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_008_neg_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 2
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_008_neg.ksh || atf_fail "Testcase failed"
}
zpool_add_008_neg_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_add_009_neg cleanup
zpool_add_009_neg_head()
{
	atf_set "descr" "'zpool add' should fail if vdevs are the same or vdev iscontained in the given pool."
	atf_set "require.progs" "ksh93 zpool"
	atf_set "timeout" 2400
}
zpool_add_009_neg_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 2
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/zpool_add_009_neg.ksh || atf_fail "Testcase failed"
}
zpool_add_009_neg_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}

# Regression test for PR 225546.  "zpool add" asserts if the pool contains a
# replacing vdev with a spare child.
# Assertion failed: (nvlist_lookup_string(cnv, "path", &path) == 0), file /usr/home/alans/freebsd/head/cddl/contrib/opensolaris/cmd/zpool/zpool_vdev.c, line 694. /usr/tests/sys/cddl/zfs/tests/cli_root/zpool_add/zpool_add_010_pos.ksh[54]: log_must[69]: log_pos: line 206: 27710: Abort(coredump)
atf_test_case zpool_add_010_pos cleanup
zpool_add_010_pos_head()
{
	atf_set "descr" "'zpool add' can add devices, even if a replacing vdev with a spare child is present"
	atf_set "require.progs" "ksh93 zpool"
}
zpool_add_010_pos_body()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	verify_disk_count "$DISKS" 5
	ksh93 $(atf_get_srcdir)/zpool_add_010_pos.ksh || atf_fail "Testcase failed"
}
zpool_add_010_pos_cleanup()
{
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_add.kshlib
	. $(atf_get_srcdir)/zpool_add.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case zpool_add_001_pos
	atf_add_test_case zpool_add_002_pos
	atf_add_test_case zpool_add_003_pos
	atf_add_test_case zpool_add_004_pos
	atf_add_test_case zpool_add_005_pos
	atf_add_test_case zpool_add_006_pos
	atf_add_test_case zpool_add_007_neg
	atf_add_test_case zpool_add_008_neg
	atf_add_test_case zpool_add_009_neg
	atf_add_test_case zpool_add_010_pos
}
