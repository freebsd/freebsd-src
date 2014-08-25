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
# ident	"@(#)cleanup.ksh	1.3	08/11/03 SMI"
#

. $STF_SUITE/tests/hotplug/hotplug.kshlib

if ! $LOFIADM -f > /dev/null 2>&1; then
	log_unsupported
fi

verify_runnable "global"

cleanup_testenv $TESTPOOL
log_must destroy_lofi_device $ALL_FILES

#
# record repair to resource(s) to cleanup fma faulty
#
log_must repair_faulty

if [[ -d $VDEV_DIR ]]; then
	log_must $RM -rf $VDEV_DIR
fi

log_pass
