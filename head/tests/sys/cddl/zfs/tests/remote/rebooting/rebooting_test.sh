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


atf_test_case rebooting_001_pos cleanup
rebooting_001_pos_head()
{
	atf_set "descr" "Verify a system can be rebooted normally after mkdir/rm operations."
	atf_set "require.progs"  rcp rsh
	atf_set "timeout" 2400
}
rebooting_001_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/rebooting.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	ksh93 $(atf_get_srcdir)/rebooting_001_pos.ksh || atf_fail "Testcase failed"
}
rebooting_001_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/rebooting.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case rebooting_001_pos
}
