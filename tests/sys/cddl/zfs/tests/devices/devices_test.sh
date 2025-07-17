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


atf_test_case devices_001_pos cleanup
devices_001_pos_head()
{
	atf_set "descr" "Setting devices=on on file system, the devices files in this filesystem can be used."
	atf_set "require.progs" "ksh93 zfs"
}
devices_001_pos_body()
{
	atf_expect_fail "The devices property is not yet supported on FreeBSD"
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/devices_common.kshlib
	. $(atf_get_srcdir)/devices.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/devices_001_pos.ksh || atf_fail "Testcase failed"
}
devices_001_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/devices_common.kshlib
	. $(atf_get_srcdir)/devices.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case devices_002_neg cleanup
devices_002_neg_head()
{
	atf_set "descr" "Setting devices=off on file system, the devices files in this filesystem can not be used."
	atf_set "require.progs" "ksh93 zfs"
}
devices_002_neg_body()
{
	atf_expect_fail "The devices property is not yet supported on FreeBSD"
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/devices_common.kshlib
	. $(atf_get_srcdir)/devices.cfg

	verify_disk_count "$DISKS" 1
	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/devices_002_neg.ksh || atf_fail "Testcase failed"
}
devices_002_neg_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/devices_common.kshlib
	. $(atf_get_srcdir)/devices.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case devices_003_pos cleanup
devices_003_pos_head()
{
	atf_set "descr" "Writing random data into /dev/zfs should do no harm."
}
devices_003_pos_body()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/devices_common.kshlib
	. $(atf_get_srcdir)/devices.cfg

	ksh93 $(atf_get_srcdir)/devices_003_pos.ksh || atf_fail "Testcase failed"
}
devices_003_pos_cleanup()
{
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/devices_common.kshlib
	. $(atf_get_srcdir)/devices.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case devices_001_pos
	atf_add_test_case devices_002_neg
	atf_add_test_case devices_003_pos
}
