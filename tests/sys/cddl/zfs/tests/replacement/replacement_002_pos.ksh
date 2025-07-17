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
# Copyright 2014 Spectra Logic Corporation.
# Use is subject to license terms.
#

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/replacement/replacement.kshlib

verify_runnable "global"

child_pids=""
log_onexit replacement_cleanup
set_disks

log_assert "Attaching a disk during I/O completes for mirrors and stripes."
for pooltype in "" "mirror"; do
	pool_action "$pooltype" attach log_must log_must
done

log_note "Verify 'zpool attach' fails for RAIDZ."
for pooltype in "raidz1" "raidz2"; do
	pool_action "$pooltype" attach log_mustnot log_mustnot
done

log_pass
