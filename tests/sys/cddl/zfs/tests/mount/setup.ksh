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

DISK=${DISKS%% *}

create_pool $TESTPOOL "$DISK" 

log_note Create file systems with mountpoints, so they are mounted automatically
i=1
TESTFSS=""
TESTDIRS=""
while [ $i -le $FS_CNT ] ; do
	dir=$TESTDIR.$i
	fs=$TESTPOOL/$TESTFS.$i

	log_pos $RM -rf $dir || log_unresolved Could not remove $dir

	log_pos $MKDIR -p $dir || log_unresolved Could not create $dir

	TESTDIRS="$TESTDIRS $dir"
	
	log_must $ZFS create $fs
	log_must $ZFS set mountpoint=$dir $fs

	TESTFSS="$TESTFSS $fs"

	log_note Make sure file system $fs was mounted 
	mounted $fs || log_fail File system $fs is not mounted

	log_note Unmount the file system 
	log_must $ZFS unmount $fs

	log_note Make sure file system $fs is unmounted
	unmounted $fs || log_fail File system $fs is not unmounted	

	(( i = i + 1 )) 	 
done
	
log_pass
