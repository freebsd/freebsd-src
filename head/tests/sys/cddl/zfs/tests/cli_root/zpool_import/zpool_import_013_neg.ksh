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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)zpool_import_013_neg.ksh	1.1	07/10/09 SMI"
#

. $STF_SUITE/include/libtest.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: zpool_import_013_neg
#
# DESCRIPTION:
#	For pool may be in use from other system, 
#	'zpool import' will prompt the warning and fails.
#
# STRATEGY:
#	1. Prepare rawfile that are created from other system.
#	2. Verify 'zpool import' will fail.
#	3. Verify 'zpool import -f' succeed.
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2007-07-05)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

if (( ZPOOL_VERSION < 6 )); then
	log_unsupported "This case need zpool version >= 6"
fi

function create_old_pool
{
	VERSION=$1
	POOL_FILES=$($ENV | grep "ZPOOL_VERSION_${VERSION}_FILES"\
		| $AWK -F= '{print $2}')
	POOL_NAME=$($ENV|grep "ZPOOL_VERSION_${VERSION}_NAME"\
		| $AWK -F= '{print $2}')

	log_note "Creating $POOL_NAME from $POOL_FILES"
	for pool_file in $POOL_FILES; do
		$CP $STF_SUITE/tests/cli_root/zpool_upgrade/blockfiles/$pool_file.Z \
		/$TESTPOOL
		$UNCOMPRESS /$TESTPOOL/$pool_file.Z
	done
	return 0
}

function cleanup
{
	if [[ -z $POOL_NAME ]]; then
		return 1
	fi
	if poolexists $POOL_NAME; then
		log_must $ZPOOL destroy $POOL_NAME
	fi
	for file in $POOL_FILES; do
		if [[ -e /$TESTPOOL/$file ]]; then
			$RM /$TESTPOOL/$file
		fi
	done
	return 0
}

log_assert "'zpool import' fail while pool may be in use from other system," \
	"it need import forcefully."
log_onexit cleanup

typeset POOL_FILES
typeset POOL_NAME
# $CONFIGS gets set in the .cfg script
for config in $CONFIGS
do
	create_old_pool $config
	log_mustnot $ZPOOL import -d /$TESTPOOL $POOL_NAME
	log_must $ZPOOL import -d /$TESTPOOL -f $POOL_NAME
	destroy_upgraded_pool
done

log_pass "'zpool import' fail while pool may be in use from other system," \
	"import forcefully succeed as expected."
