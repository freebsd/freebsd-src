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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"@(#)mdb_001_pos.ksh	1.4	08/11/03 SMI"
#

. $STF_SUITE/include/libtest.kshlib

#################################################################################
#
# __stc_assertion_start
#
# ID: mdb_001_pos
#
# DESCRIPTION:
#	Verify that the ZFS mdb dcmds and walkers are working as expected.
#
# STRATEGY:
#	1) Given a list of dcmds and walkers
#	2) Step through each element of the list
#	3) Verify the output by checking for "mdb:" in the output string
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING_STATUS: COMPLETED (2006-10-11)
#
# __stc_assertion_end
#
################################################################################

verify_runnable "global"

set -A dcmds "::walk spa" \
	"::walk spa | ::spa " \
	"::walk spa | ::spa -c" \
	"::walk spa | ::spa -v" \
	"::walk spa | ::spa_config" \
	"::walk spa | ::spa_verify" \
	"::walk spa | ::spa_space" \
	"::walk spa | ::spa_space -b" \
	"::walk spa | ::spa_vdevs" \
	"::walk spa | ::walk metaslab" \
	"::walk spa | ::print struct spa spa_root_vdev | ::vdev" \
	"::walk spa | ::print struct spa spa_root_vdev | ::vdev -re" \
	"::dbufs" \
	"::dbufs -n mos -o mdn -l 0 -b 0" \
	"::dbufs | ::dbuf" \
	"::dbuf_stats" \
	"::abuf_find 1 2" \
	"0x2FFFFF::zio_pipeline"
#
# The commands above were supplied by the ZFS development team. The idea is to
# do as much checking as possible without the need to hardcode addresses.
#
# 0x2FFFFF::zio_pipeline - The dcmd converts the number to an ASCII string so
# we pass the maximum value to the dcmd to ensure all pipeline commands are 
# listed.
# 

#
# Append Solaris 5.11 specific dcmds
#
typeset -i i=${#dcmds[*]}
if check_version "5.11" ; then
        for str in  "::walk spa | ::print -a struct spa spa_uberblock.ub_rootbp | ::blkptr" \
            "::walk spa | ::print -a struct spa spa_dsl_pool->dp_dirty_datasets | ::walk txg_list" \
            "::walk spa | ::walk zms_freelist"
        do
                dcmds[$i]="$str"

                ((i = i + 1))
        done
else
        for str in  "::walk spa | ::walk zms_freelist"
        do
                dcmds[$i]="$str"

                ((i = i + 1))
        done
fi


log_assert "Verify that the ZFS mdb dcmds and walkers are working as expected."

typeset -i RET=0

$RM -f $OUTFILE > /dev/null 2>&1

i=0
while (( $i < ${#dcmds[*]} )); do
	log_note "Verifying: '${dcmds[i]}'"
        $ECHO "${dcmds[i]}" | $MDB -k > $OUTFILE 2>&1
	RET=$?
	if (( $RET != 0 )); then
		log_fail "mdb '${dcmds[i]}' returned error $RET"
	fi

	#
	# mdb prefixes all errors with "mdb: " so we check the output.
	#
	$GREP "mdb:" $OUTFILE > /dev/null 2>&1
	RET=$?
	if (( $RET == 0 )); then
		$ECHO "mdb '${dcmds[i]}' contained 'mdb:'"
		# Using $TAIL limits the number of lines in the log
		$TAIL -100 $OUTFILE
		log_fail "mdb walker or dcmd failed"
	fi

        ((i = i + 1))
done

log_pass "The ZFS mdb dcmds and walkers are working as expected."
