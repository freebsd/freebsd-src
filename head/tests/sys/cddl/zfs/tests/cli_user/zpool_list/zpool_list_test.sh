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


atf_test_case zpool_list_001_pos cleanup
zpool_list_001_pos_head()
{
	atf_set "descr" "zpool list [-H] [-o filed[,filed]*] [<pool_name> ...]"
	atf_set "require.progs"  zpool
	atf_set "require.user" root
}
zpool_list_001_pos_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	atf_expect_fail 'BUG26173: zpool man page and STF tests were never updated for removal of "used" property'

	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_list.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	su -m `atf_config_get unprivileged_user` -c "ksh93 $(atf_get_srcdir)/zpool_list_001_pos.ksh" || atf_fail "Testcase failed"
}
zpool_list_001_pos_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_list.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_test_case zpool_list_002_neg cleanup
zpool_list_002_neg_head()
{
	atf_set "descr" "Executing 'zpool list' with bad options fails"
	atf_set "require.progs"  zpool
	atf_set "require.user" root
}
zpool_list_002_neg_body()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_list.cfg

	ksh93 $(atf_get_srcdir)/setup.ksh || atf_fail "Setup failed"
	su -m `atf_config_get unprivileged_user` -c "ksh93 $(atf_get_srcdir)/zpool_list_002_neg.ksh" || atf_fail "Testcase failed"
}
zpool_list_002_neg_cleanup()
{
	export TESTCASE_ID=$(echo $(atf_get ident) | cksum -o 2 | cut -f 1 -d " ")
	. $(atf_get_srcdir)/../../../include/default.cfg
	. $(atf_get_srcdir)/zpool_list.cfg

	ksh93 $(atf_get_srcdir)/cleanup.ksh || atf_fail "Cleanup failed"
}


atf_init_test_cases()
{

	atf_add_test_case zpool_list_001_pos
	atf_add_test_case zpool_list_002_neg
}
