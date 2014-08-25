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


atf_test_case rootpool_001_pos cleanup
rootpool_001_pos_head()
{
	atf_set "descr" "rootpool's bootfs property must be equal to <rootfs>"
	atf_set "require.config"  is_zfs_root
}
rootpool_001_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_001_pos.ksh || atf_fail "Testcase failed"
}
rootpool_001_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case rootpool_002_neg cleanup
rootpool_002_neg_head()
{
	atf_set "descr" "zpool/zfs destory <rootpool> should return error"
	atf_set "require.config"  is_zfs_root
	atf_set "require.progs"  zfs zpool
}
rootpool_002_neg_body()
{
	atf_expect_fail "Destroying the root pool will panic FreeBSD BUG25145"
	atf_fail "Prematurely fail the test so we don't cause a panic"
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_002_neg.ksh || atf_fail "Testcase failed"
}
rootpool_002_neg_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case rootpool_003_neg cleanup
rootpool_003_neg_head()
{
	atf_set "descr" "system related filesytems can not be renamed or destroyed"
	atf_set "require.config"  is_zfs_root
	atf_set "require.progs"  zfs
}
rootpool_003_neg_body()
{
	atf_expect_fail "Destroying the root pool will panic FreeBSD BUG25145"
	atf_fail "Prematurely fail the test so we don't cause a panic"
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_003_neg.ksh || atf_fail "Testcase failed"
}
rootpool_003_neg_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case rootpool_004_pos cleanup
rootpool_004_pos_head()
{
	atf_set "descr" "rootfs's canmount property must be noauto"
	atf_set "require.config"  is_zfs_root
}
rootpool_004_pos_body()
{
	atf_skip "The expected behavior of this test does not match the behavior of Illumos.  Illumos and FreeBSD behave identically in this regard, so the test is probably wrong"
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_004_pos.ksh || atf_fail "Testcase failed"
}
rootpool_004_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case rootpool_005_pos cleanup
rootpool_005_pos_head()
{
	atf_set "descr" "rootpool/ROOT's mountpoint must be legacy"
	atf_set "require.config"  is_zfs_root
}
rootpool_005_pos_body()
{
	atf_skip "FreeBSD does not place any special requirements on <rootpool>/ROOT"
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_005_pos.ksh || atf_fail "Testcase failed"
}
rootpool_005_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case rootpool_006_pos cleanup
rootpool_006_pos_head()
{
	atf_set "descr" "zfs rootfs's mountpoint must be mounted and must be /"
	atf_set "require.config"  is_zfs_root
}
rootpool_006_pos_body()
{
	atf_skip "The expected behavior of this test does not match the behavior of Illumos.  Illumos and FreeBSD behave identically in this regard, so the test is probably wrong"
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_006_pos.ksh || atf_fail "Testcase failed"
}
rootpool_006_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case rootpool_007_neg cleanup
rootpool_007_neg_head()
{
	atf_set "descr" "the zfs rootfs's compression property can not set to gzip and gzip[1-9]"
	atf_set "require.config"  is_zfs_root
	atf_set "require.progs"  zfs
}
rootpool_007_neg_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/rootpool_007_neg.ksh || atf_fail "Testcase failed"
}
rootpool_007_neg_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case rootpool_001_pos
	atf_add_test_case rootpool_002_neg
	atf_add_test_case rootpool_003_neg
	atf_add_test_case rootpool_004_pos
	atf_add_test_case rootpool_005_pos
	atf_add_test_case rootpool_006_pos
	atf_add_test_case rootpool_007_neg
}
