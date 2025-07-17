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
# Copyright 2013 Spectra Logic Corp.  All rights reserved.
# Use is subject to license terms.
#
#

# This is the setup script for ZFSD tests that are based on the hotspare
# framework.  It is almost identical to tests/hotspare/setup.ksh,
# but does not stop ZFSD.

. $STF_SUITE/include/libtest.kshlib
. $STF_SUITE/tests/hotspare/hotspare.kshlib

verify_runnable "global"

log_must cleanup_devices_all

log_pass
