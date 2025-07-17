#!/usr/local/bin/ksh93 -p
#
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
# Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.

. $STF_SUITE/tests/delegate/delegate_common.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: zfs_allow_003_pos
#
# DESCRIPTION:
#	Verify option '-l' only allow permission to the dataset itself.
#
# STRATEGY:
#	1. Create descendent datasets of $ROOT_TESTFS
#	2. Select user, group and everyone and set local permission separately.
#	3. Set locally permissions to $ROOT_TESTFS or $ROOT_TESTVOL.
#	4. Verify the permissions are only allow on $ROOT_TESTFS or
#	   $ROOT_TESTVOL.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-09-19)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "both"

log_assert "Verify option '-l' only allow permission to the dataset itself."

childfs=$ROOT_TESTFS/childfs

eval set -A dataset $DATASETS
enc=$(get_prop encryption $dataset)
if [[ $? -eq 0 ]] && [[ -n "$enc" ]] && [[ "$enc" != "off" ]]; then
	typeset perms="snapshot,reservation,compression,allow,\
userprop"
else
	typeset perms="snapshot,reservation,compression,checksum,\
allow,userprop"
fi

log_must $ZFS create $childfs

for dtst in $DATASETS ; do
	log_must $ZFS allow -l $STAFF1 $perms $dtst
	log_must verify_perm $dtst $perms $STAFF1
	if [[ $dtst == $ROOT_TESTFS ]] ; then
		log_must verify_noperm $childfs $perms \
			$STAFF1 $STAFF2 $OTHER1 $OTHER2
	fi
done

log_must restore_root_datasets

log_must $ZFS create $childfs
for dtst in $DATASETS ; do
	log_must $ZFS allow -l -g $STAFF_GROUP $perms $dtst
	log_must verify_perm $dtst $perms $STAFF1 $STAFF2
	if [[ $dtst == $ROOT_TESTFS ]] ; then
		log_must verify_noperm $childfs $perms \
			$STAFF1 $STAFF2 $OTHER1 $OTHER2
	fi
done

log_must restore_root_datasets

log_must $ZFS create $childfs
for dtst in $DATASETS ; do
	log_must $ZFS allow -l -e $perms $dtst
	log_must verify_perm $dtst $perms $STAFF1 $STAFF2 $OTHER1 $OTHER2
	if [[ $dtst == $ROOT_TESTFS ]] ; then
		log_must verify_noperm $childfs $perms \
			$STAFF1 $STAFF2 $OTHER1 $OTHER2
	fi
done

log_pass "Verify option '-l' only allow permission to the dataset itself pass."
