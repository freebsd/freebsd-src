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


atf_test_case hotplug_001_pos cleanup
hotplug_001_pos_head()
{
	atf_set "descr" "When removing a device from a redundant pool, the device'sstate will be indicated as 'REMOVED'. No FMA faulty message."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_001_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_001_pos.ksh || atf_fail "Testcase failed"
}
hotplug_001_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_002_pos cleanup
hotplug_002_pos_head()
{
	atf_set "descr" "When removing and reinserting a device, the device status isONLINE with no FMA errors."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_002_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_002_pos.ksh || atf_fail "Testcase failed"
}
hotplug_002_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_003_pos cleanup
hotplug_003_pos_head()
{
	atf_set "descr" "Having removed a device from a redundant pool and inserted a newdevice, the new device state will be 'ONLINE' when autoreplace is on,\and 'UNAVAIL' when autoreplace is off"
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_003_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_003_pos.ksh || atf_fail "Testcase failed"
}
hotplug_003_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_004_pos cleanup
hotplug_004_pos_head()
{
	atf_set "descr" "When device replacement fails, the original device's state willbe 'UNAVAIL' and an FMA fault will be generated."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_004_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_004_pos.ksh || atf_fail "Testcase failed"
}
hotplug_004_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_005_pos cleanup
hotplug_005_pos_head()
{
	atf_set "descr" "Regarding of autoreplace, when removing offline device andreinserting again. This device's status is 'ONLINE'.  \No FMA fault was generated."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_005_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_005_pos.ksh || atf_fail "Testcase failed"
}
hotplug_005_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_006_pos cleanup
hotplug_006_pos_head()
{
	atf_set "descr" "When unsetting/setting autoreplace, then replacing device, verifythe device's status is 'UNAVAIL/ONLINE'. No FMA fault is generated."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_006_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_006_pos.ksh || atf_fail "Testcase failed"
}
hotplug_006_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_007_pos cleanup
hotplug_007_pos_head()
{
	atf_set "descr" "When autoreplace is 'on', replacing the device with a smaller one.Verify the device's status is 'UNAVAIL'. FMA fault has been generated."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_007_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_007_pos.ksh || atf_fail "Testcase failed"
}
hotplug_007_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_008_pos cleanup
hotplug_008_pos_head()
{
	atf_set "descr" "When removing hotspare device, verify device status is 'REMOVED'."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm
	atf_set "timeout" 1800
}
hotplug_008_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_008_pos.ksh || atf_fail "Testcase failed"
}
hotplug_008_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_009_pos cleanup
hotplug_009_pos_head()
{
	atf_set "descr" "Power off machine and replacing device, verify device status isONLINE when autoreplace is on and UNAVAIL when autoreplace is off"
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm svcadm svcs
	atf_set "timeout" 1800
}
hotplug_009_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_009_pos.ksh || atf_fail "Testcase failed"
}
hotplug_009_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_010_pos cleanup
hotplug_010_pos_head()
{
	atf_set "descr" "Removing device offlined and reinserting onlined,verify the device status ONLINE."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm svcadm svcs
	atf_set "timeout" 1800
}
hotplug_010_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_010_pos.ksh || atf_fail "Testcase failed"
}
hotplug_010_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case hotplug_011_pos cleanup
hotplug_011_pos_head()
{
	atf_set "descr" "Removing device offlined, verify device status is UNAVAIL,when the system is onlined."
	atf_set "require.config" rt_long
	atf_set "require.progs"  mkfile zpool lofiadm svcadm svcs
	atf_set "timeout" 1800
}
hotplug_011_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/hotplug_011_pos.ksh || atf_fail "Testcase failed"
}
hotplug_011_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../include/default.cfg
	. $(atf_get_srcdir)/hotplug.kshlib
	. $(atf_get_srcdir)/hotplug.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case hotplug_001_pos
	atf_add_test_case hotplug_002_pos
	atf_add_test_case hotplug_003_pos
	atf_add_test_case hotplug_004_pos
	atf_add_test_case hotplug_005_pos
	atf_add_test_case hotplug_006_pos
	atf_add_test_case hotplug_007_pos
	atf_add_test_case hotplug_008_pos
	atf_add_test_case hotplug_009_pos
	atf_add_test_case hotplug_010_pos
	atf_add_test_case hotplug_011_pos
}
