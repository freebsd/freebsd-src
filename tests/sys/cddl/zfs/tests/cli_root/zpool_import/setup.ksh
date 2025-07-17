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

verify_runnable "global"

create_pool "$TESTPOOL" "$DISK0"

if [[ -d $TESTDIR ]]; then
	$RM -rf $TESTDIR  || log_unresolved Could not remove $TESTDIR
	$MKDIR -p $TESTDIR || log_unresolved Could not create $TESTDIR
fi

log_must $ZFS create $TESTPOOL/$TESTFS
log_must $ZFS set mountpoint=$TESTDIR $TESTPOOL/$TESTFS

[[ ! -d $DEVICE_DIR ]] && \
	log_must $MKDIR -p $DEVICE_DIR

i=0
while (( i < $MAX_NUM )); do
	log_must create_vdevs ${DEVICE_DIR}/${DEVICE_FILE}$i
	(( i = i + 1 ))
done

log_pass
