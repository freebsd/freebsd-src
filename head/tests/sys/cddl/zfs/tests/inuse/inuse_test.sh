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


atf_test_case inuse_001_pos
inuse_001_pos_head()
{
	atf_set "descr" "Ensure ZFS cannot use a device designated as a dump device"
	atf_set "require.config" rt_short
	atf_set "require.progs"  dumpadm zpool
	atf_set "timeout" 1200
}
inuse_001_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_001_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_002_pos
inuse_002_pos_head()
{
	atf_set "descr" "Ensure ZFS does not interfere with devices in use by SVM"
	atf_set "require.config" rt_short
	atf_set "require.progs"  metainit metadb metastat zpool metaclear
	atf_set "timeout" 1200
}
inuse_002_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_002_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_003_pos
inuse_003_pos_head()
{
	atf_set "descr" "Ensure ZFS does not interfere with devices that are in use byufsdump or ufsrestore"
	atf_set "require.config" rt_short
	atf_set "require.progs"  zpool ufsrestore ufsdump
	atf_set "timeout" 1200
}
inuse_003_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_003_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_004_pos
inuse_004_pos_head()
{
	atf_set "descr" "format will disallow modification of a mounted zfs disk partition or a spare device"
	atf_set "require.config" rt_short
	atf_set "require.progs"  zfs zpool format
	atf_set "timeout" 1200
}
inuse_004_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_004_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_005_pos
inuse_005_pos_head()
{
	atf_set "descr" "Verify newfs over active pool fails."
	atf_set "require.config" rt_medium
	atf_set "timeout" 1200
}
inuse_005_pos_body()
{
	atf_expect_fail "REQ25571 ZFS does not open geoms in exclusive mode"
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_005_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_006_pos
inuse_006_pos_head()
{
	atf_set "descr" "Verify dumpadm over active pool fails."
	atf_set "require.config" rt_short
	atf_set "require.progs"  dumpadm
	atf_set "timeout" 1200
}
inuse_006_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_006_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_007_pos
inuse_007_pos_head()
{
	atf_set "descr" "Verify dumpadm over exported pool succeed."
	atf_set "require.config" rt_short
	atf_set "require.progs"  dumpadm zpool
	atf_set "timeout" 1200
}
inuse_007_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_007_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_008_pos
inuse_008_pos_head()
{
	atf_set "descr" "Verify newfs over exported pool succeed."
	atf_set "require.config" rt_short
	atf_set "require.progs"  zpool
	atf_set "timeout" 1200
}
inuse_008_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_008_pos.ksh || atf_fail "Testcase failed"
}



atf_test_case inuse_009_pos
inuse_009_pos_head()
{
	atf_set "descr" "Verify format over exported pool succeed."
	atf_set "require.config" rt_short
	atf_set "require.progs"  zpool
	atf_set "timeout" 1200
}
inuse_009_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/inuse.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/inuse_009_pos.ksh || atf_fail "Testcase failed"
}



atf_init_test_cases()
{

	atf_add_test_case inuse_001_pos
	atf_add_test_case inuse_002_pos
	atf_add_test_case inuse_003_pos
	atf_add_test_case inuse_004_pos
	atf_add_test_case inuse_005_pos
	atf_add_test_case inuse_006_pos
	atf_add_test_case inuse_007_pos
	atf_add_test_case inuse_008_pos
	atf_add_test_case inuse_009_pos
}
