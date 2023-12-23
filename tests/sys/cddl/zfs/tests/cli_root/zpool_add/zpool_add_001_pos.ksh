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
. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/cli_root/zpool_add/zpool_add.kshlib

################################################################################
#
# __stc_assertion_start
#
# ID: zpool_add_001_pos
#
# DESCRIPTION: 
# 	'zpool add <pool> <vdev> ...' can successfully add the specified 
# devices to the given pool
#
# STRATEGY:
#	1. Create a storage pool
#	2. Add spare devices to the pool
#	3. Verify the devices are added to the pool successfully
#
# TESTABILITY: explicit
#
# TEST_AUTOMATION_LEVEL: automated
#
# CODING STATUS: COMPLETED (2005-09-27)
#
# __stc_assertion_end
#
###############################################################################

verify_runnable "global"

log_assert "'zpool add <pool> <vdev> ...' can add devices to the pool." 

set -A keywords "" "mirror" "raidz" "raidz1" "spare"

set_disks

typeset diskname0=${DISK0#/dev/}
typeset diskname1=${DISK1#/dev/}
typeset diskname2=${DISK2#/dev/}
typeset diskname3=${DISK3#/dev/}
typeset diskname4=${DISK4#/dev/}

pooldevs="${diskname0}\
	 \"/dev/${diskname0} ${diskname1}\" \
	 \"${diskname0} ${diskname1} ${diskname2}\""
mirrordevs="\"/dev/${diskname0} ${diskname1}\""
raidzdevs="\"/dev/${diskname0} ${diskname1}\""

typeset -i i=0
typeset vdev
eval set -A poolarray $pooldevs
eval set -A mirrorarray $mirrordevs
eval set -A raidzarray $raidzdevs

while (( $i < ${#keywords[*]} )); do
        case ${keywords[i]} in
        ""|spare)     
		for vdev in "${poolarray[@]}"; do
			create_pool "$TESTPOOL" "${diskname3}"
			log_must poolexists "$TESTPOOL"
                	log_must $ZPOOL add -f "$TESTPOOL" ${keywords[i]} \
				$vdev
			log_must iscontained "$TESTPOOL" "$vdev"
			destroy_pool "$TESTPOOL"
		done
		
		;;
        mirror) 
		for vdev in "${mirrorarray[@]}"; do
			create_pool "$TESTPOOL" "${keywords[i]}" \
				"${diskname3}" "${diskname4}"
			log_must poolexists "$TESTPOOL"
                	log_must $ZPOOL add "$TESTPOOL" ${keywords[i]} \
				$vdev
			log_must iscontained "$TESTPOOL" "$vdev"
			destroy_pool "$TESTPOOL"
		done
		
		;;
        raidz|raidz1)  
		for vdev in "${raidzarray[@]}"; do
			create_pool "$TESTPOOL" "${keywords[i]}" \
				"${diskname3}" "${diskname4}"
			log_must poolexists "$TESTPOOL"
                	log_must $ZPOOL add "$TESTPOOL" ${keywords[i]} \
				$vdev
			log_must iscontained "$TESTPOOL" "$vdev"
			destroy_pool "$TESTPOOL"
		done
		
		;;
        esac 

        (( i = i+1 ))
done

log_pass "'zpool add <pool> <vdev> ...' executes successfully"
